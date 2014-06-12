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

/// Create an HTTP connection object.
///
/// @param server Server to send HTTP requests to.
/// @param assert_user Assert user in header?
/// @param stat_name Name of statistic to report connection info to.
/// @param load_monitor Load Monitor.
/// @param lvc Statistics last value cache.
/// @param sas_log_level the level to log HTTP flows at (protocol/detail).
HttpConnection::HttpConnection(const std::string& server,
                               bool assert_user,
                               const std::string& stat_name,
                               LoadMonitor* load_monitor,
                               LastValueCache* lvc,
                               SASEvent::HttpLogLevel sas_log_level) :
  _server(server),
  _assert_user(assert_user),
  _sas_log_level(sas_log_level)
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
/// @param sas_log_level the level to log HTTP flows at (protocol/detail).
HttpConnection::HttpConnection(const std::string& server,
                               bool assert_user,
                               SASEvent::HttpLogLevel sas_log_level) :
  _server(server),
  _assert_user(assert_user),
  _sas_log_level(sas_log_level)
{
  pthread_key_create(&_curl_thread_local, cleanup_curl);
  pthread_key_create(&_uuid_thread_local, cleanup_uuid);
  pthread_mutex_init(&_lock, NULL);
  curl_global_init(CURL_GLOBAL_DEFAULT);
  _statistic = NULL;
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

    // We always talk to the same server, unless we intentionally want
    // to rotate our requests. So a connection pool makes no sense.
    curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, 1L);

    // Maximum time to wait for a response.
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, TOTAL_TIMEOUT_MS);

    // Time to wait until we establish a TCP connection to one of the
    // available addresses.  We will try the first address for half of
    // this time.
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 2 * SINGLE_CONNECT_TIMEOUT_MS);

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


  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &doc);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

  // Create a UUID to use for SAS correlation. Log it to SAS and add it to the
  // HTTP request we are about to send.
  boost::uuids::uuid uuid = get_random_uuid();
  std::string uuid_str = boost::uuids::to_string(uuid);
  SAS::Marker corr_marker(trail, MARKER_ID_VIA_BRANCH_PARAM, 0);
  corr_marker.add_var_param(uuid_str);
  SAS::report_marker(corr_marker, SAS::Marker::Scope::Trace);
  extra_headers = curl_slist_append(extra_headers,
                                    (SASEvent::HTTP_BRANCH_HEADER_NAME + ": " + uuid_str).c_str());

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
  bool first_error_503 = false;

  // Try to get a decent connection. We may need to retry, but only
  // once - cURL itself does most of the retrying for us.
  for (int attempt = 0; attempt < 2; attempt++)
  {
    curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, recycle_conn ? 1L : 0L);

    // Report the request to SAS.
    event_id = ((_sas_log_level == SASEvent::HttpLogLevel::PROTOCOL) ?
                   SASEvent::TX_HTTP_REQ : SASEvent::TX_HTTP_REQ_DETAIL);
    SAS::Event tx_http_req(trail, event_id,  0);
    tx_http_req.add_var_param(method_str);
    tx_http_req.add_var_param(url);
    tx_http_req.add_var_param(body);
    SAS::report_event(tx_http_req);

    // Send the request.
    doc.clear();
    LOG_DEBUG("Sending HTTP request : %s (try %d) %s", url.c_str(), attempt, (recycle_conn) ? "on new connection" : "");
    rc = curl_easy_perform(curl);

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
      LOG_DEBUG("Received HTTP error response : %s : %s", url.c_str(), curl_easy_strerror(rc));

      char* remote_ip;
      curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &remote_ip);

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
        http_err.add_var_param(url);
        http_err.add_static_param(rc);
        http_err.add_var_param(curl_easy_strerror(rc));
        http_err.add_var_param(remote_ip);
        SAS::report_event(http_err);
      }

      bool error_is_503 = ((rc == CURLE_HTTP_RETURNED_ERROR) && (http_rc == 503));

      // Is this an error we should retry? If cURL itself has already
      // retried (e.g., CURLE_COULDNT_CONNECT) then there is no point
      // in us retrying. But if the remote application has hung
      // (CURLE_OPERATION_TIMEDOUT) or a previously-up connection has
      // failed (CURLE_SEND|RECV_ERROR) then we must retry once
      // ourselves.
      bool non_fatal = ((rc == CURLE_OPERATION_TIMEDOUT) ||
                        (rc == CURLE_SEND_ERROR) ||
                        (rc == CURLE_RECV_ERROR) ||
                        (error_is_503));

      if ((non_fatal) && (attempt == 0))
      {
        // Loop around and try again.  Always request a fresh connection.
        LOG_ERROR("%s failed at server %s : %s (%d %d) : retrying",
                  url.c_str(), remote_ip, curl_easy_strerror(rc), rc, http_rc);
        recycle_conn = true;

        // Record that the first error was a 503 error.
        if (error_is_503)
        {
          first_error_503 = true;
        }
      }
      else
      {
        // Fatal error or we've already retried once - we're done!
        LOG_ERROR("%s failed at server %s : %s (%d %d) : fatal",
                  url.c_str(), remote_ip, curl_easy_strerror(rc), rc, http_rc);

        // Check whether we should apply a penalty. We do this when:
        //  - both attempts return 503 errors, which means the downstream
        // node is overloaded/requests to it are timeing.
        //  - the error is a 504, which means that the node downsteam of the node
        // we're connecting to currently has reported that it is overloaded/was
        // unresponsive.
        if (((error_is_503 && first_error_503) ||
             ((rc == CURLE_HTTP_RETURNED_ERROR) && (http_rc == HTTP_GATEWAY_TIMEOUT))) &&
            (_load_monitor != NULL))
        {
          _load_monitor->incr_penalties();
        }

        break;
      }
    }
  }

  if ((rc == CURLE_OK) || (rc == CURLE_HTTP_RETURNED_ERROR))
  {
    char* remote_ip;
    CURLcode rc = curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &remote_ip);

    if (rc == CURLE_OK)
    {
      entry->set_remote_ip(remote_ip);
    }
    else
    {
      entry->set_remote_ip("UNKNOWN");  // LCOV_EXCL_LINE Can't happen.
    }
  }
  else
  {
    entry->set_remote_ip("");
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
  // Get the next desired inter-arrival time. Choose this
  // randomly so as to avoid spikes.
  unsigned long interval_ms = (unsigned long)_rand();

  if ((_deadline_ms == 0L) ||
      ((_deadline_ms + interval_ms) < now_ms))
  {
    // This is the first request, or the next arrival has
    // already passed (in which case things must be pretty
    // quiet). Just bump the next deadline into the future.
    _deadline_ms = now_ms + interval_ms;
  }
  else
  {
    // The next arrival is yet to come. Schedule it relative to
    // the last intended time, so as not to skew the mean
    // upwards.
    _deadline_ms += interval_ms;
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
  rx_http_rsp.add_var_param(url);
  rx_http_rsp.add_var_param(doc);
  rx_http_rsp.add_static_param(http_rc);
  SAS::report_event(rx_http_rsp);
}
