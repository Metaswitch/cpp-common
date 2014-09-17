/**
 * @file httpconnection.cpp HttpConnection class methods.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include <curl/curl.h>
#include <cassert>
#include <iostream>
#include <map>

#include "utils.h"
#include "log.h"
#include "sas.h"
#include "httpconnection.h"
#include "load_monitor.h"
#include "random_uuid.h"

/// Total time to wait for a response from the server before giving
/// up.  This is the value that affects the user experience, so should
/// be set to what we consider acceptable.  Covers lookup, possibly
/// multiple connection attempts, request, and response.  In
/// milliseconds.
static const long TOTAL_TIMEOUT_MS = 500;

/// Approximate length of time to wait before giving up on a
/// connection attempt to a single address (in milliseconds).  cURL
/// may wait more or less than this depending on the number of
/// addresses to be tested and where this address falls in the
/// sequence. A connection will take longer than this to establish if
/// multiple addresses must be tried. This includes only the time to
/// perform the DNS lookup and establish the connection, not to send
/// the request or receive the response.
///
/// We set this quite short to ensure we quickly move on to another
/// server. A connection should be very fast to establish (a few
/// milliseconds) in the success case.
static const long SINGLE_CONNECT_TIMEOUT_MS = 50;

/// Mean age of a connection before we recycle it. Ensures we respect
/// DNS changes, and that we rebalance load when servers come back up
/// after failure. Actual connection recycle events are
/// Poisson-distributed with this mean inter-arrival time.
static const double CONNECTION_AGE_MS = 60 * 1000.0;

/// Duration to blacklist hosts after we fail to connect to them.
static const int BLACKLIST_DURATION = 30;

/// Maximum number of targets to try connecting to.
static const int MAX_TARGETS = 5;

/// Create an HTTP connection object.
///
/// @param server Server to send HTTP requests to.
/// @param assert_user Assert user in header?
/// @param resolver HTTP resolver to use to resolve server's IP addresses
/// @param stat_name Name of statistic to report connection info to.
/// @param load_monitor Load Monitor.
/// @param lvc Statistics last value cache.
/// @param sas_log_level the level to log HTTP flows at (protocol/detail).
HttpConnection::HttpConnection(const std::string& server,
                               bool assert_user,
                               HttpResolver* resolver,
                               const std::string& stat_name,
                               LoadMonitor* load_monitor,
                               LastValueCache* lvc,
                               SASEvent::HttpLogLevel sas_log_level) :
  _server(server),
  _host(host_from_server(server)),
  _port(port_from_server(server)),
  _assert_user(assert_user),
  _resolver(resolver),
  _sas_log_level(sas_log_level),
  _comm_monitor(NULL)
{
  pthread_key_create(&_curl_thread_local, cleanup_curl);
  pthread_key_create(&_uuid_thread_local, cleanup_uuid);
  pthread_mutex_init(&_lock, NULL);
  curl_global_init(CURL_GLOBAL_DEFAULT);
  std::vector<std::string> no_stats;
  _statistic = new Statistic(stat_name, lvc);
  _statistic->report_change(no_stats);
  _load_monitor = load_monitor;
}

/// Create an HTTP connection object.
///
/// @param server Server to send HTTP requests to.
/// @param assert_user Assert user in header?
/// @param resolver HTTP resolver to use to resolve server's IP addresses
/// @param sas_log_level the level to log HTTP flows at (protocol/detail).
HttpConnection::HttpConnection(const std::string& server,
                               bool assert_user,
                               HttpResolver* resolver,
                               SASEvent::HttpLogLevel sas_log_level) :
  _server(server),
  _host(host_from_server(server)),
  _port(port_from_server(server)),
  _assert_user(assert_user),
  _resolver(resolver),
  _sas_log_level(sas_log_level),
  _comm_monitor(NULL)
{
  pthread_key_create(&_curl_thread_local, cleanup_curl);
  pthread_key_create(&_uuid_thread_local, cleanup_uuid);
  pthread_mutex_init(&_lock, NULL);
  curl_global_init(CURL_GLOBAL_DEFAULT);
  _statistic = NULL;
  _load_monitor = NULL;
}

HttpConnection::~HttpConnection()
{
  // Clean up this thread's connection now, rather than waiting for
  // pthread_exit.  This is to support use by single-threaded code
  // (e.g., UTs), where pthread_exit is never called.
  CURL* curl = pthread_getspecific(_curl_thread_local);
  if (curl != NULL)
  {
    pthread_setspecific(_curl_thread_local, NULL);
    cleanup_curl(curl); curl = NULL;
  }

  RandomUUIDGenerator* uuid_gen =
    (RandomUUIDGenerator*)pthread_getspecific(_uuid_thread_local);

  if (uuid_gen != NULL)
  {
    pthread_setspecific(_uuid_thread_local, NULL);
    cleanup_uuid(uuid_gen); uuid_gen = NULL;
  }

  if (_statistic != NULL)
  {
    delete _statistic;
    _statistic = NULL;
  }
}

/// Set a monitor to track HTTP REST communication state, and set/clear
/// alarms based upon recent activity.
void HttpConnection::set_comm_monitor(CommunicationMonitor* comm_monitor)
{
  _comm_monitor = comm_monitor;
}

/// Get the thread-local curl handle if it exists, and create it if not.
CURL* HttpConnection::get_curl_handle()
{
  CURL* curl = pthread_getspecific(_curl_thread_local);
  if (curl == NULL)
  {
    curl = curl_easy_init();
    LOG_DEBUG("Allocated CURL handle %p", curl);
    pthread_setspecific(_curl_thread_local, curl);

    // Create our private data
    PoolEntry* entry = new PoolEntry(this);
    curl_easy_setopt(curl, CURLOPT_PRIVATE, entry);

    // Retrieved data will always be written to a string.
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &string_store);

    // Tell cURL to fail on 400+ response codes.
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    // Only keep one TCP connection to a Homestead per thread, to
    // avoid using unnecessary resources. We only try a different
    // Homestead when one fails, or after we've had it open for a
    // while, and in neither case do we want to keep the old
    // connection around.
    curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, 1L);

    // Maximum time to wait for a response.
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, TOTAL_TIMEOUT_MS);

    // Time to wait until we establish a TCP connection to a single host.
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, SINGLE_CONNECT_TIMEOUT_MS);

    // We mustn't reuse DNS responses, because cURL does no shuffling
    // of DNS entries and we rely on this for load balancing.
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 0L);

    // Nagle is not required. Probably won't bite us, but can't hurt
    // to turn it off.
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);

    // We are a multithreaded app using C-Ares. This is the
    // recommended setting.
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  }
  return curl;
}

// Map the CURLcode into a sensible HTTP return code.
HTTPCode HttpConnection::curl_code_to_http_code(CURL* curl, CURLcode code)
{
  switch (code)
  {
  case CURLE_OK:
    return HTTP_OK;
  // LCOV_EXCL_START
  case CURLE_URL_MALFORMAT:
  case CURLE_NOT_BUILT_IN:
    return HTTP_BAD_RESULT;
  // LCOV_EXCL_STOP
  case CURLE_REMOTE_FILE_NOT_FOUND:
    return HTTP_NOT_FOUND;
  // LCOV_EXCL_START
  case CURLE_COULDNT_RESOLVE_PROXY:
  case CURLE_COULDNT_RESOLVE_HOST:
  case CURLE_COULDNT_CONNECT:
  case CURLE_AGAIN:
    return HTTP_NOT_FOUND;
  case CURLE_HTTP_RETURNED_ERROR:
    // We have an actual HTTP error available, so use that.
  {
    long http_code = 0;
    CURLcode rc = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    assert(rc == CURLE_OK);
    return http_code;
  }
  default:
    return HTTP_SERVER_ERROR;
  // LCOV_EXCL_STOP
  }
}

// Reset the cURL handle to a default state, so that settings from one
// request don't leak into another
void HttpConnection::reset_curl_handle(CURL* curl)
{
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, NULL);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, NULL);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, NULL);
  curl_easy_setopt(curl, CURLOPT_POST, 0);
}

HTTPCode HttpConnection::send_delete(const std::string& path,
                                     SAS::TrailId trail,
                                     const std::string& body)
{
  std::string unused_response;
  std::map<std::string, std::string> unused_headers;

  return send_delete(path, unused_headers, unused_response, trail, body);
}

HTTPCode HttpConnection::send_delete(const std::string& path,
                                     SAS::TrailId trail,
                                     const std::string& body,
                                     std::string& response)
{
  std::map<std::string, std::string> unused_headers;

  return send_delete(path, unused_headers, response, trail, body);
}

HTTPCode HttpConnection::send_delete(const std::string& path,
                                     std::map<std::string, std::string>& headers,
                                     std::string& response,
                                     SAS::TrailId trail,
                                     const std::string& body,
                                     const std::string& username)
{
  CURL *curl = get_curl_handle();
  struct curl_slist *slist = NULL;
  slist = curl_slist_append(slist, "Expect:");

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

  HTTPCode status = send_request(path, body, response, username, trail, "DELETE", curl);

  curl_slist_free_all(slist);

  return status;
}

HTTPCode HttpConnection::send_put(const std::string& path,
                                  const std::string& body,
                                  SAS::TrailId trail,
                                  const std::string& username)
{
  std::string unused_response;
  std::map<std::string, std::string> unused_headers;
  return HttpConnection::send_put(path, unused_headers, unused_response, body, trail, username);
}

HTTPCode HttpConnection::send_put(const std::string& path,
                                  std::string& response,
                                  const std::string& body,
                                  SAS::TrailId trail,
                                  const std::string& username)
{
  std::map<std::string, std::string> unused_headers;
  return HttpConnection::send_put(path, unused_headers, response, body, trail, username);
}

HTTPCode HttpConnection::send_put(const std::string& path,
                                  std::map<std::string, std::string>& headers,
                                  const std::string& body,
                                  SAS::TrailId trail,
                                  const std::string& username)
{
  std::string unused_response;
  return HttpConnection::send_put(path, headers, unused_response, body, trail, username);
}

HTTPCode HttpConnection::send_put(const std::string& path,                     //< Absolute path to request from server - must start with "/"
                                  std::map<std::string, std::string>& headers, //< Map of headers from the response
                                  std::string& response,                       //< Retrieved document
                                  const std::string& body,                     //< Body to send in request
                                  SAS::TrailId trail,                          //< SAS trail
                                  const std::string& username)                 //< Username to assert (if assertUser was true, else ignored)
{
  CURL *curl = get_curl_handle();
  struct curl_slist *slist = NULL;
  slist = curl_slist_append(slist, "Content-Type: application/json");

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &HttpConnection::write_headers);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, &headers);

  HTTPCode status = send_request(path, body, response, "", trail, "PUT", curl);

  curl_slist_free_all(slist);

  return status;
}

HTTPCode HttpConnection::send_post(const std::string& path,
                                   std::map<std::string, std::string>& headers,
                                   const std::string& body,
                                   SAS::TrailId trail,
                                   const std::string& username)
{
  std::string unused_response;
  return HttpConnection::send_post(path, headers, unused_response, body, trail, username);
}

HTTPCode HttpConnection::send_post(const std::string& path,                     //< Absolute path to request from server - must start with "/"
                                   std::map<std::string, std::string>& headers, //< Map of headers from the response
                                   std::string& response,                       //< Retrieved document
                                   const std::string& body,                     //< Body to send in request
                                   SAS::TrailId trail,                          //< SAS trail
                                   const std::string& username)                 //< Username to assert (if assertUser was true, else ignored).
{
  CURL *curl = get_curl_handle();
  struct curl_slist *slist = NULL;
  slist = curl_slist_append(slist, "Content-Type: application/json");

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
  curl_easy_setopt(curl, CURLOPT_POST, 1);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &HttpConnection::write_headers);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, &headers);

  HTTPCode status = send_request(path, body, response, username, trail, "POST", curl);

  curl_slist_free_all(slist);

  return status;
}

/// Get data; return a HTTP return code
HTTPCode HttpConnection::send_get(const std::string& path,
                                  std::string& response,
                                  const std::string& username,
                                  SAS::TrailId trail)
{
  std::map<std::string, std::string> unused_headers;
  return HttpConnection::send_get(path, unused_headers, response, username, trail);
}

/// Get data; return a HTTP return code
HTTPCode HttpConnection::send_get(const std::string& path,                     //< Absolute path to request from server - must start with "/"
                                  std::map<std::string, std::string>& headers, //< Map of headers from the response
                                  std::string& response,                       //< Retrieved document
                                  const std::string& username,                 //< Username to assert (if assertUser was true, else ignored)
                                  SAS::TrailId trail)                          //< SAS trail
{
  CURL *curl = get_curl_handle();

  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);

  return send_request(path, "", response, username, trail, "GET", curl);
}

/// Get data; return a HTTP return code
HTTPCode HttpConnection::send_request(const std::string& path,       //< Absolute path to request from server - must start with "/"
                                      std::string body,              //< Body to send on the request
                                      std::string& doc,              //< OUT: Retrieved document
                                      const std::string& username,   //< Username to assert (if assertUser was true, else ignored).
                                      SAS::TrailId trail,            //< SAS trail to use
                                      const std::string& method_str, // The method, used for logging.
                                      CURL* curl)
{
  std::string url = "http://" + _server + path;
  struct curl_slist *extra_headers = NULL;
  PoolEntry* entry;
  int event_id;
  CURLcode rc = curl_easy_getinfo(curl, CURLINFO_PRIVATE, (char**)&entry);
  assert(rc == CURLE_OK);

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &doc);

  if (!body.empty())
  {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  }

  // Create a UUID to use for SAS correlation and add it to the HTTP message.
  boost::uuids::uuid uuid = get_random_uuid();
  std::string uuid_str = boost::uuids::to_string(uuid);
  extra_headers = curl_slist_append(extra_headers,
                                    (SASEvent::HTTP_BRANCH_HEADER_NAME + ": " + uuid_str).c_str());

  // Now log the marker to SAS. Flag that SAS should not reactivate the trail
  // group as a result of associations on this marker (doing so after the call
  // ends means it will take a long time to be searchable in SAS).
  SAS::Marker corr_marker(trail, MARKER_ID_VIA_BRANCH_PARAM, 0);
  corr_marker.add_var_param(uuid_str);
  SAS::report_marker(corr_marker, SAS::Marker::Scope::Trace, false);

  // Add the user's identity (if required).
  if (_assert_user)
  {
    extra_headers = curl_slist_append(extra_headers,
                                      ("X-XCAP-Asserted-Identity: " + username).c_str());
  }
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, extra_headers);

  // Determine whether to recycle the connection, based on
  // previously-calculated deadline.
  struct timespec tp;
  int rv = clock_gettime(CLOCK_MONOTONIC, &tp);
  assert(rv == 0);
  unsigned long now_ms = tp.tv_sec * 1000 + (tp.tv_nsec / 1000000);
  bool recycle_conn = entry->is_connection_expired(now_ms);

  // Resolve the host.
  std::vector<AddrInfo> targets;
  _resolver->resolve(_host, _port, MAX_TARGETS, targets, trail);

  // If we're not recycling the connection, try to get the current connection
  // IP address and add it to the front of the target list.
  if (!recycle_conn)
  {
    char* primary_ip;
    if (curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &primary_ip) == CURLE_OK)
    {
      AddrInfo ai;
      _resolver->parse_ip_target(primary_ip, ai.address);
      ai.port = (_port != 0) ? _port : 80;
      ai.transport = IPPROTO_TCP;

      targets.erase(std::remove(targets.begin(), targets.end(), ai), targets.end());
      targets.insert(targets.begin(), ai);
    }
  }

  // If the list of targets only contains 1 target, clone it - we always want
  // to retry at least once.
  if (targets.size() == 1)
  {
    targets.push_back(targets[0]);
  }

  // Report the request to SAS.
  event_id = ((_sas_log_level == SASEvent::HttpLogLevel::PROTOCOL) ?
                 SASEvent::TX_HTTP_REQ : SASEvent::TX_HTTP_REQ_DETAIL);
  SAS::Event tx_http_req(trail, event_id,  0);
  tx_http_req.add_var_param(method_str);
  tx_http_req.add_var_param(Utils::url_unescape(url));
  tx_http_req.add_var_param(body);
  SAS::report_event(tx_http_req);

  // Track the number of HTTP 503 and 504 responses and the number of timeouts
  // or I/O errors.
  int num_http_503_responses = 0;
  int num_http_504_responses = 0;
  int num_timeouts_or_io_errors = 0;

  // Track the IP addresses we're connecting to.  If we fail, we failed to
  // resolve the host, so default to that.
  const char *remote_ip = NULL;
  rc = CURLE_COULDNT_RESOLVE_HOST;

  // Try to get a decent connection - try each of the hosts in turn (although
  // we might quit early if we have too many HTTP-level failures).
  for (std::vector<AddrInfo>::const_iterator i = targets.begin();
       i != targets.end();
       ++i)
  {
    curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, recycle_conn ? 1L : 0L);

    // Convert the target IP address into a string and fix up the URL.  It
    // would be nice to use curl_easy_setopt(CURL_RESOLVE) here, but its
    // implementation is incomplete.
    char buf[100];
    remote_ip = inet_ntop(i->address.af, &i->address.addr, buf, sizeof(buf));
    std::string ip_url;
    if (i->address.af == AF_INET6)
    {
      ip_url = "http://[" + std::string(remote_ip) + "]:" + std::to_string(i->port) + path;
    }
    else
    {
      ip_url = "http://" + std::string(remote_ip) + ":" + std::to_string(i->port) + path;
    }
    curl_easy_setopt(curl, CURLOPT_URL, ip_url.c_str());

    // Send the request.
    doc.clear();
    LOG_DEBUG("Sending HTTP request : %s (trying %s) %s", url.c_str(), remote_ip, (recycle_conn) ? "on new connection" : "");
    rc = curl_easy_perform(curl);

    // If we performed an HTTP transaction (successfully or otherwise, get the
    // return code.
    long http_rc = 0;
    if ((rc == CURLE_OK) || (rc == CURLE_HTTP_RETURNED_ERROR))
    {
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_rc);
    }

    if (rc == CURLE_OK)
    {
      LOG_DEBUG("Received HTTP response : %s", doc.c_str());

      // Report the response to SAS.
      sas_log_http_rsp(trail, curl, http_rc, method_str, url, doc, 0);

      if (recycle_conn)
      {
        entry->update_deadline(now_ms);
      }

      // Success!
      break;
    }
    else
    {
      LOG_ERROR("%s failed at server %s : %s (%d %d) : fatal",
                url.c_str(), remote_ip, curl_easy_strerror(rc), rc, http_rc);

      if (rc == CURLE_HTTP_RETURNED_ERROR)
      {
        // Report the response to SAS
        sas_log_http_rsp(trail, curl, http_rc, method_str, url, doc, 0);
      }
      else
      {
        // Report the error to SAS.
        event_id = ((_sas_log_level == SASEvent::HttpLogLevel::PROTOCOL) ?
                       SASEvent::HTTP_REQ_ERROR : SASEvent::HTTP_REQ_ERROR_DETAIL);
        SAS::Event http_err(trail, event_id, 0);
        http_err.add_var_param(method_str);
        http_err.add_var_param(Utils::url_unescape(url));
        http_err.add_static_param(rc);
        http_err.add_var_param(curl_easy_strerror(rc));
        http_err.add_var_param(remote_ip);
        SAS::report_event(http_err);
      }

      // If we forced a new connection and we failed even to establish an HTTP
      // connection, blacklist this IP address.
      if (recycle_conn &&
          (rc != CURLE_HTTP_RETURNED_ERROR) &&
          (rc != CURLE_REMOTE_FILE_NOT_FOUND) &&
          (rc != CURLE_REMOTE_ACCESS_DENIED))
      {
        _resolver->blacklist(*i, BLACKLIST_DURATION);
      }

      // Determine the failure mode and update the correct counter.
      bool fatal_http_error = false;
      if (rc == CURLE_HTTP_RETURNED_ERROR)
      {
        if (http_rc == 503)
        {
          num_http_503_responses++;
        }
        // LCOV_EXCL_START fakecurl doesn't let us return custom return codes.
        else if (http_rc == 504)
        {
          num_http_504_responses++;
        }
        else
        {
          fatal_http_error = true;
        }
        // LCOV_EXCL_STOP
      }
      else if ((rc == CURLE_REMOTE_FILE_NOT_FOUND) ||
               (rc == CURLE_REMOTE_ACCESS_DENIED))
      {
        fatal_http_error = true;
      }
      else if ((rc == CURLE_OPERATION_TIMEDOUT) ||
               (rc == CURLE_SEND_ERROR) ||
               (rc == CURLE_RECV_ERROR))
      {
        num_timeouts_or_io_errors++;
      }

      // Decide whether to keep trying.
      if ((num_http_503_responses + num_timeouts_or_io_errors >= 2) ||
          (num_http_504_responses >= 1) ||
          fatal_http_error ||
          (rc == CURLE_COULDNT_CONNECT))
      {
        break;
      }
    }
  }

  // Check whether we should apply a penalty. We do this when:
  //  - both attempts return 503 errors, which means the downstream node is
  //    overloaded/requests to it are timeing.
  //  - the error is a 504, which means that the node downsteam of the node
  //    we're connecting to currently has reported that it is overloaded/was
  //    unresponsive.
  if (((num_http_503_responses >= 2) ||
       (num_http_504_responses >= 1)) &&
      (_load_monitor != NULL))
  {
    _load_monitor->incr_penalties();
  }

  if ((rc == CURLE_OK) || (rc == CURLE_HTTP_RETURNED_ERROR))
  {
    entry->set_remote_ip(remote_ip);

    if (_comm_monitor)
    {
      if (num_http_503_responses >= 2)
      {
        _comm_monitor->inform_failure(now_ms); // LCOV_EXCL_LINE - No UT for 503 fails
      }
      else
      {
        _comm_monitor->inform_success(now_ms);
      }
    }
  }
  else
  {
    entry->set_remote_ip("");

    if (_comm_monitor)
    {
      _comm_monitor->inform_failure(now_ms);
    }
  }

  HTTPCode http_code = curl_code_to_http_code(curl, rc);
  if ((rc != CURLE_OK) && (rc != CURLE_REMOTE_FILE_NOT_FOUND))
  {
    LOG_ERROR("cURL failure with cURL error code %d (see man 3 libcurl-errors) and HTTP error code %ld", (int)rc, http_code);  // LCOV_EXCL_LINE
  }

  reset_curl_handle(curl);
  curl_slist_free_all(extra_headers);
  return http_code;
}

/// cURL helper - write data into string.
size_t HttpConnection::string_store(void* ptr, size_t size, size_t nmemb, void* stream)
{
  ((std::string*)stream)->append((char*)ptr, size * nmemb);
  return (size * nmemb);
}


/// Called to clean up the cURL handle.
void HttpConnection::cleanup_curl(void* curlptr)
{
  CURL* curl = (CURL*)curlptr;

  PoolEntry* entry;
  CURLcode rc = curl_easy_getinfo(curl, CURLINFO_PRIVATE, (char**)&entry);
  if (rc == CURLE_OK)
  {
    // Connection has closed.
    entry->set_remote_ip("");
    delete entry;
  }

  curl_easy_cleanup(curl);
}


/// PoolEntry constructor
HttpConnection::PoolEntry::PoolEntry(HttpConnection* parent) :
  _parent(parent),
  _deadline_ms(0L),
  _rand(1.0 / CONNECTION_AGE_MS)
{
}


/// PoolEntry destructor
HttpConnection::PoolEntry::~PoolEntry()
{
}


/// Is it time to recycle the connection? Expects CLOCK_MONOTONIC
/// current time, in milliseconds.
bool HttpConnection::PoolEntry::is_connection_expired(unsigned long now_ms)
{
  return (now_ms > _deadline_ms);
}


/// Update deadline to next appropriate value. Expects
/// CLOCK_MONOTONIC current time, in milliseconds.  Call on
/// successful connection.
void HttpConnection::PoolEntry::update_deadline(unsigned long now_ms)
{
  // Get the next desired inter-arrival time. Take a random sample
  // from an exponential distribution so as to avoid spikes.
  unsigned long interval_ms = (unsigned long)_rand();

  if ((_deadline_ms == 0L) ||
      ((_deadline_ms + interval_ms) < now_ms))
  {
    // This is the first request, or the new arrival time has
    // already passed (in which case things must be pretty
    // quiet). Just bump the next deadline into the future.
    _deadline_ms = now_ms + interval_ms;
  }
  else
  {
    // The new arrival time is yet to come. Schedule it relative to
    // the last intended time, so as not to skew the mean
    // upwards.

    // We don't recycle any connections in the UTs. (We could do this
    // by manipulating time, but would have no way of checking it
    // worked.)
    _deadline_ms += interval_ms; // LCOV_EXCL_LINE
  }
}


/// Set the remote IP, and update statistics.
void HttpConnection::PoolEntry::set_remote_ip(const std::string& value)  //< Remote IP, or "" if no connection.
{
  if (value == _remote_ip)
  {
    return;
  }

  pthread_mutex_lock(&_parent->_lock);

  if (!_remote_ip.empty())
  {
    // Decrement the number of connections to this address.
    if (--_parent->_server_count[_remote_ip] <= 0)
    {
      // No more connections to this address, so remove it from the map.
      _parent->_server_count.erase(_remote_ip);
    }
  }

  if (!value.empty())
  {
    // Increment the count of connections to this address.  (Note this is
    // safe even if this is the first connection as the [] operator will
    // insert an entry initialised to 0.)
    ++_parent->_server_count[value];
  }

  _remote_ip = value;

  // Now build the statistics to report.
  std::vector<std::string> new_value;

  for (std::map<std::string, int>::iterator iter = _parent->_server_count.begin();
       iter != _parent->_server_count.end();
       ++iter)
  {
    new_value.push_back(iter->first);
    new_value.push_back(std::to_string(iter->second));
  }

  pthread_mutex_unlock(&_parent->_lock);

  // Actually report outside the mutex to avoid any risk of deadlock.
  if (_parent->_statistic != NULL)
  {
    _parent->_statistic->report_change(new_value);
  }
}

size_t HttpConnection::write_headers(void *ptr, size_t size, size_t nmemb, std::map<std::string, std::string> *headers)
{
  char* headerLine = reinterpret_cast<char *>(ptr);

  // convert to string
  std::string headerString(headerLine, (size * nmemb));

  std::string key;
  std::string val;

  // find colon
  size_t colon_loc = headerString.find(":");
  if (colon_loc == std::string::npos)
  {
    key = headerString;
    val = "";
  }
  else
  {
    key = headerString.substr(0, colon_loc);
    val = headerString.substr(colon_loc + 1, std::string::npos);
  }

  // Lowercase the key (for consistency) and remove spaces
  std::transform(key.begin(), key.end(), key.begin(), ::tolower);
  key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());
  val.erase(std::remove_if(val.begin(), val.end(), ::isspace), val.end());

  LOG_DEBUG("Received header %s with value %s", key.c_str(), val.c_str());
  (*headers)[key] = val;

  return size * nmemb;
}

void HttpConnection::cleanup_uuid(void *uuid_gen)
{
  delete (RandomUUIDGenerator*)uuid_gen; uuid_gen = NULL;
}

boost::uuids::uuid HttpConnection::get_random_uuid()
{
  // Get the factory from thread local data (creating it if it doesn't exist).
  RandomUUIDGenerator* uuid_gen;
  uuid_gen =
    (RandomUUIDGenerator*)pthread_getspecific(_uuid_thread_local);

  if (uuid_gen == NULL)
  {
    uuid_gen = new RandomUUIDGenerator();
    pthread_setspecific(_uuid_thread_local, uuid_gen);
  }

  // _uuid_gen_ is a pointer to a callable object that returns a UUID.
  return (*uuid_gen)();
}

void HttpConnection::sas_log_http_rsp(SAS::TrailId trail,
                                      CURL* curl,
                                      long http_rc,
                                      const std::string& method_str,
                                      const std::string& url,
                                      const std::string& doc,
                                      uint32_t instance_id)
{
  int event_id = ((_sas_log_level == SASEvent::HttpLogLevel::PROTOCOL) ?
                    SASEvent::RX_HTTP_RSP : SASEvent::RX_HTTP_RSP_DETAIL);
  SAS::Event rx_http_rsp(trail, event_id, instance_id);
  rx_http_rsp.add_var_param(method_str);
  rx_http_rsp.add_var_param(Utils::url_unescape(url));
  rx_http_rsp.add_var_param(doc);
  rx_http_rsp.add_static_param(http_rc);
  SAS::report_event(rx_http_rsp);
}

void HttpConnection::host_port_from_server(const std::string& server, std::string& host, int& port)
{
  std::string server_copy = server;
  Utils::trim(server_copy);
  int colon_idx;
  if (((server_copy[0] != '[') ||
       (server_copy[server_copy.length() - 1] != ']')) &&
      ((colon_idx = server_copy.find_last_of(':')) != std::string::npos))
  {
    host = server_copy.substr(0, colon_idx);
    port = stoi(server_copy.substr(colon_idx + 1));
  }
  else
  {
    host = server_copy;
    port = 0;
  }
}

std::string HttpConnection::host_from_server(const std::string& server)
{
  std::string host;
  int port;
  host_port_from_server(server, host, port);
  return host;
}

int HttpConnection::port_from_server(const std::string& server)
{
  std::string host;
  int port;
  host_port_from_server(server, host, port);
  return port;
}
