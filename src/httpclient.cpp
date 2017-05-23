/**
 * @file httpclient.cpp HttpClient class methods.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <curl/curl.h>
#include <cassert>
#include <iostream>
#include <map>

#include "cpp_common_pd_definitions.h"
#include "utils.h"
#include "log.h"
#include "sas.h"
#include "httpclient.h"
#include "load_monitor.h"
#include "random_uuid.h"

/// Maximum number of targets to try connecting to.
static const int MAX_TARGETS = 5;

/// Create an HTTP client object.
///
/// @param assert_user Assert user in header?
/// @param resolver HTTP resolver to use to resolve server's IP addresses
/// @param stat_name SNMP table to report connection info to.
/// @param load_monitor Load Monitor.
/// @param sas_log_level the level to log HTTP flows at (none/protocol/detail).
HttpClient::HttpClient(bool assert_user,
                       HttpResolver* resolver,
                       SNMP::IPCountTable* stat_table,
                       LoadMonitor* load_monitor,
                       SASEvent::HttpLogLevel sas_log_level,
                       BaseCommunicationMonitor* comm_monitor,
                       bool should_omit_body) :
  _assert_user(assert_user),
  _resolver(resolver),
  _load_monitor(load_monitor),
  _sas_log_level(sas_log_level),
  _comm_monitor(comm_monitor),
  _stat_table(stat_table),
  _conn_pool(load_monitor, stat_table),
  _should_omit_body(should_omit_body)
{
  pthread_key_create(&_uuid_thread_local, cleanup_uuid);
  pthread_mutex_init(&_lock, NULL);
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

/// Create an HTTP client object.
///
/// @param assert_user Assert user in header?
/// @param resolver HTTP resolver to use to resolve server's IP addresses
/// @param sas_log_level the level to log HTTP flows at (none/protocol/detail).
HttpClient::HttpClient(bool assert_user,
                       HttpResolver* resolver,
                       SASEvent::HttpLogLevel sas_log_level,
                       BaseCommunicationMonitor* comm_monitor) :
  HttpClient(assert_user,
             resolver,
             NULL,
             NULL,
             sas_log_level,
             comm_monitor)
{
}

HttpClient::~HttpClient()
{
  RandomUUIDGenerator* uuid_gen =
    (RandomUUIDGenerator*)pthread_getspecific(_uuid_thread_local);

  if (uuid_gen != NULL)
  {
    pthread_setspecific(_uuid_thread_local, NULL);
    cleanup_uuid(uuid_gen); uuid_gen = NULL;
  }

  pthread_key_delete(_uuid_thread_local);
}

// Map the CURLcode into a sensible HTTP return code.
HTTPCode HttpClient::curl_code_to_http_code(CURL* curl, CURLcode code)
{
  switch (code)
  {
  case CURLE_OK:
  {
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    return http_code;
  // LCOV_EXCL_START
  }
  case CURLE_URL_MALFORMAT:
  case CURLE_NOT_BUILT_IN:
    return HTTP_BAD_REQUEST;
  // LCOV_EXCL_STOP
  case CURLE_REMOTE_FILE_NOT_FOUND:
    return HTTP_NOT_FOUND;
  // LCOV_EXCL_START
  case CURLE_COULDNT_RESOLVE_PROXY:
  case CURLE_COULDNT_RESOLVE_HOST:
  case CURLE_COULDNT_CONNECT:
  case CURLE_AGAIN:
    return HTTP_NOT_FOUND;
  case CURLE_OPERATION_TIMEDOUT:
    return HTTP_SERVER_UNAVAILABLE;
  default:
    return HTTP_SERVER_ERROR;
  // LCOV_EXCL_STOP
  }
}

HTTPCode HttpClient::send_delete(const std::string& url,
                                 SAS::TrailId trail,
                                 const std::string& body)
{
  std::string unused_response;
  std::map<std::string, std::string> unused_headers;

  return send_delete(url, unused_headers, unused_response, trail, body);
}

HTTPCode HttpClient::send_delete(const std::string& url,
                                 SAS::TrailId trail,
                                 const std::string& body,
                                 std::string& response)
{
  std::map<std::string, std::string> unused_headers;

  return send_delete(url, unused_headers, response, trail, body);
}

HTTPCode HttpClient::send_delete(const std::string& url,
                                 std::map<std::string, std::string>& headers,
                                 std::string& response,
                                 SAS::TrailId trail,
                                 const std::string& body,
                                 const std::string& username)
{
  std::vector<std::string> unused_extra_headers;
  HTTPCode status = send_request(RequestType::DELETE,
                                 url,
                                 body,
                                 response,
                                 username,
                                 trail,
                                 unused_extra_headers,
                                 NULL);
  return status;
}

HTTPCode HttpClient::send_put(const std::string& url,
                              const std::string& body,
                              SAS::TrailId trail,
                              const std::string& username)
{
  std::string unused_response;
  std::map<std::string, std::string> unused_headers;
  std::vector<std::string> extra_req_headers;
  return HttpClient::send_put(url,
                              unused_headers,
                              unused_response,
                              body,
                              extra_req_headers,
                              trail,
                              username);
}

HTTPCode HttpClient::send_put(const std::string& url,
                              std::string& response,
                              const std::string& body,
                              SAS::TrailId trail,
                              const std::string& username)
{
  std::map<std::string, std::string> unused_headers;
  std::vector<std::string> extra_req_headers;
  return HttpClient::send_put(url,
                              unused_headers,
                              response,
                              body,
                              extra_req_headers,
                              trail,
                              username);
}

HTTPCode HttpClient::send_put(const std::string& url,
                              std::map<std::string, std::string>& headers,
                              const std::string& body,
                              SAS::TrailId trail,
                              const std::string& username)
{
  std::string unused_response;
  std::vector<std::string> extra_req_headers;
  return HttpClient::send_put(url,
                              headers,
                              unused_response,
                              body,
                              extra_req_headers,
                              trail,
                              username);
}

HTTPCode HttpClient::send_put(const std::string& url,
                              std::map<std::string, std::string>& headers,
                              std::string& response,
                              const std::string& body,
                              const std::vector<std::string>& extra_req_headers,
                              SAS::TrailId trail,
                              const std::string& username)
{
  HTTPCode status = send_request(RequestType::PUT,
                                 url,
                                 body,
                                 response,
                                 "",
                                 trail,
                                 extra_req_headers,
                                 &headers);
  return status;
}

HTTPCode HttpClient::send_post(const std::string& url,
                               std::map<std::string, std::string>& headers,
                               const std::string& body,
                               SAS::TrailId trail,
                               const std::string& username)
{
  std::string unused_response;
  return HttpClient::send_post(url, headers, unused_response, body, trail, username);
}

HTTPCode HttpClient::send_post(const std::string& url,
                               std::map<std::string, std::string>& headers,
                               std::string& response,
                               const std::string& body,
                               SAS::TrailId trail,
                               const std::string& username)
{
  std::vector<std::string> unused_extra_headers;
  HTTPCode status = send_request(RequestType::POST,
                                 url,
                                 body,
                                 response,
                                 username,
                                 trail,
                                 unused_extra_headers,
                                 &headers);
  return status;
}

/// Get data; return a HTTP return code
HTTPCode HttpClient::send_get(const std::string& url,
                              std::string& response,
                              const std::string& username,
                              SAS::TrailId trail)
{
  std::map<std::string, std::string> unused_rsp_headers;
  std::vector<std::string> unused_req_headers;
  return HttpClient::send_get(url, unused_rsp_headers, response, username, unused_req_headers, trail);
}

/// Get data; return a HTTP return code
HTTPCode HttpClient::send_get(const std::string& url,
                              std::string& response,
                              std::vector<std::string> headers,
                              SAS::TrailId trail)
{
  std::map<std::string, std::string> unused_rsp_headers;
  return HttpClient::send_get(url, unused_rsp_headers, response, "", headers, trail);
}

/// Get data; return a HTTP return code
HTTPCode HttpClient::send_get(const std::string& url,
                              std::map<std::string, std::string>& headers,
                              std::string& response,
                              const std::string& username,
                              SAS::TrailId trail)
{
  std::vector<std::string> unused_req_headers;
  return HttpClient::send_get(url, headers, response, username, unused_req_headers, trail);
}

/// Get data; return a HTTP return code
HTTPCode HttpClient::send_get(const std::string& url,
                              std::map<std::string, std::string>& headers,
                              std::string& response,
                              const std::string& username,
                              std::vector<std::string> headers_to_add,
                              SAS::TrailId trail)
{
  return send_request(RequestType::GET,
                      url,
                      "",
                      response,
                      username,
                      trail,
                      headers_to_add,
                      NULL);
}

std::string HttpClient::request_type_to_string(RequestType request_type)
{
  switch (request_type) {
  case RequestType::DELETE:
    return "DELETE";
  case RequestType::PUT:
    return "PUT";
  case RequestType::POST:
    return "POST";
  case RequestType::GET:
    return "GET";
  // LCOV_EXCL_START
  // The above cases are exhaustive by the definition of RequestType
  default:
    return "UNKNOWN";
  // LCOV_EXCL_STOP
  }
}

/// Get data; return a HTTP return code
HTTPCode HttpClient::send_request(RequestType request_type,
                                  const std::string& url,
                                  std::string body,
                                  std::string& doc,
                                  const std::string& username,
                                  SAS::TrailId trail,
                                  std::vector<std::string> headers_to_add,
                                  std::map<std::string, std::string>* response_headers)
{
  HTTPCode http_code;
  CURLcode rc;

  // Create a UUID to use for SAS correlation.
  boost::uuids::uuid uuid = get_random_uuid();
  std::string uuid_str = boost::uuids::to_string(uuid);

  // Now log the marker to SAS. Flag that SAS should not reactivate the trail
  // group as a result of associations on this marker (doing so after the call
  // ends means it will take a long time to be searchable in SAS).
  SAS::Marker corr_marker(trail, MARKER_ID_VIA_BRANCH_PARAM, 0);
  corr_marker.add_var_param(uuid_str);
  SAS::report_marker(corr_marker, SAS::Marker::Scope::Trace, false);

  std::string scheme;
  std::string server;
  std::string path;
  if (!Utils::parse_http_url(url, scheme, server, path))
  {
    TRC_ERROR("%s could not be parsed as a URL : fatal",
              url.c_str());
    return HTTP_BAD_REQUEST;
  }

  std::string host = host_from_server(scheme, server);
  int port = port_from_server(scheme, server);

  // Resolve the host, and check whether it was an IP address all along.
  BaseAddrIterator* target_it = _resolver->resolve_iter(host, port, trail);
  IP46Address dummy_address;
  bool host_is_ip = BaseResolver::parse_ip_target(host, dummy_address);

  // Track the number of HTTP 503 and 504 responses and the number of timeouts
  // or I/O errors.
  int num_http_503_responses = 0;
  int num_http_504_responses = 0;
  int num_timeouts_or_io_errors = 0;

  // Track the IP addresses we're connecting to.  If we fail, we failed to
  // resolve the host, so default to that.
  const char *remote_ip = NULL;
  rc = CURLE_COULDNT_RESOLVE_HOST;
  http_code = HTTP_NOT_FOUND;

  AddrInfo target;

  // Iterate over the targets returned by target_it until a successful
  // connection is made, a specified number of failures is reached, or the
  // targets are exhausted. If only one target is available, it should be tried
  // twice.
  for (int attempts = 0;
       target_it->next(target) || attempts == 1;
       ++attempts)
  {
    // Get a curl handle and the associated pool entry
    ConnectionHandle<CURL*> conn_handle = _conn_pool.get_connection(target);
    CURL* curl = conn_handle.get_connection();

    // Construct and add extra headers
    struct curl_slist* extra_headers = build_headers(headers_to_add,
                                                     _assert_user,
                                                     username,
                                                     uuid_str);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, extra_headers);

    // Set general curl options
    set_curl_options_general(curl, body, doc);

    // Set response header curl options. We always want to catch the headers,
    // even if the caller isn't interested.
    std::map<std::string, std::string> internal_rsp_hdrs;

    if (!response_headers)
    {
      response_headers = &internal_rsp_hdrs;
    }

    set_curl_options_response(curl, response_headers);

    // Set request-type specific curl options
    set_curl_options_request(curl, request_type);

    // Convert the target IP address into a string and tell curl to resolve to that.
    char buf[100];
    remote_ip = inet_ntop(target.address.af,
                          &target.address.addr,
                          buf,
                          sizeof(buf));

    // We want curl's DNS cache to contain exactly one entry: for the host and
    // IP that we're currently processing.
    //
    // It's easy to add an entry with the CURLOPT_RESOLVE flag, but removing it
    // again in anticipation of the next query is trickier - because options
    // that we set are not processed until we actually make a query, and we can
    // only set this option once per request.
    //
    // So each time we add an entry we also store a curl_slist which will
    // remove that entry, and then _next_ time round we use that as well as
    // adding the entry that we do want.
    //
    // At this point then, we retrieve the value previously stored - if any.
    // (We may be here for the very first time, or the previous query may have
    // been direct to an IP address, so we may not find anything).
    curl_slist *host_resolve = NULL;
    curl_easy_getinfo(curl, CURLINFO_PRIVATE, &host_resolve);
    curl_easy_setopt(curl, CURLOPT_PRIVATE, NULL);

    // Add the new entry - except in the case where the host is already an IP
    // address.
    if (!host_is_ip)
    {
      std::string resolve_addr =
        host + ":" + std::to_string(port) + ":" + remote_ip;
      host_resolve = curl_slist_append(host_resolve, resolve_addr.c_str());
      TRC_DEBUG("Set CURLOPT_RESOLVE: %s", resolve_addr.c_str());
    }
    if (host_resolve != NULL)
    {
      curl_easy_setopt(curl, CURLOPT_RESOLVE, host_resolve);
    }

    // Set the curl target URL
    std::string curl_target = scheme + "://" + host + ":" + std::to_string(port) + path;
    curl_easy_setopt(curl, CURLOPT_URL, curl_target.c_str());

    // Create and register an object to record the HTTP transaction.
    Recorder recorder;
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &recorder);


    // Set host-specific curl options
    void* host_context = set_curl_options_host(curl, host, port);

    // Get the current timestamp before calling into curl.  This is because we
    // can't log the request to SAS until after curl_easy_perform has returned.
    // This could be a long time if the server is being slow, and we want to log
    // the request with the right timestamp.
    SAS::Timestamp req_timestamp = SAS::get_current_timestamp();

    // Send the request.
    doc.clear();
    TRC_DEBUG("Sending HTTP request : %s (trying %s)", url.c_str(), remote_ip);
    rc = curl_easy_perform(curl);

    // If a request was sent, log it to SAS.
    std::string method_str = request_type_to_string(request_type);
    if (recorder.request.length() > 0)
    {
      sas_log_http_req(trail, curl, method_str, url, recorder.request, req_timestamp, 0);
    }

    // Clean up from setting up the DNS cache this time round.
    if (host_resolve != NULL)
    {
      curl_slist_free_all(host_resolve);
      host_resolve = NULL;
    }

    // Prepare to remove the DNS entry from curl's cache next time round, if
    // necessary
    if (!host_is_ip)
    {
      std::string resolve_remove_addr =
        std::string("-") + host + ":" + std::to_string(port);
      host_resolve = curl_slist_append(NULL, resolve_remove_addr.c_str());
      curl_easy_setopt(curl, CURLOPT_PRIVATE, host_resolve);
    }

    // Log the result of the request.
    long http_rc = 0;
    if (rc == CURLE_OK)
    {
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_rc);
      sas_log_http_rsp(trail, curl, http_rc, method_str, url, recorder.response, 0);
      TRC_DEBUG("Received HTTP response: status=%d, doc=%s", http_rc, doc.c_str());
    }
    else
    {
      TRC_WARNING("%s failed at server %s : %s (%d) : fatal",
                  url.c_str(), remote_ip, curl_easy_strerror(rc), rc);
      sas_log_curl_error(trail, remote_ip, target.port, method_str, url, rc, 0);
    }

    http_code = curl_code_to_http_code(curl, rc);

    // At this point, we are finished with the curl object, so it is safe to
    // free the headers
    curl_slist_free_all(extra_headers);

    // Clean up any memory allocated by set_curl_options_host
    cleanup_host_context(host_context);

    // Update the connection recycling and retry algorithms.
    if ((rc == CURLE_OK) && !(http_rc >= 400))
    {
      // Success!
      _resolver->success(target);
      break;
    }
    else
    {
      // If we failed to even to establish an HTTP connection or recieved a 503
      // with a Retry-After header, blacklist this IP address.
      if ((!(http_rc >= 400)) &&
          (rc != CURLE_REMOTE_FILE_NOT_FOUND) &&
          (rc != CURLE_REMOTE_ACCESS_DENIED))
      {
        // The CURL connection should not be returned to the pool
        TRC_DEBUG("Blacklist on connection failure");
        conn_handle.set_return_to_pool(false);
        _resolver->blacklist(target);
      }
      else if (http_rc == 503)
      {
        // Check for a Retry-After header on 503 responses and if present with
        // a valid value (i.e. an integer) blacklist the host for the given
        // number of seconds.
        TRC_DEBUG("Have 503 failure");
        std::map<std::string, std::string>::iterator retry_after_header =
                                          response_headers->find("retry-after");
        int retry_after = 0;

        if (retry_after_header != response_headers->end())
        {
          TRC_DEBUG("Try to parse retry after value");
          std::string retry_after_val = retry_after_header->second;
          retry_after = atoi(retry_after_val.c_str());

          // Log if we failed to parse the Retry-After header here
          if (retry_after == 0)
          {
            TRC_WARNING("Failed to parse Retry-After value: %s", retry_after_val.c_str());
            sas_log_bad_retry_after_value(trail, retry_after_val, 0);
          }
        }

        if (retry_after > 0)
        {
          // The CURL connection should not be returned to the pool
          TRC_DEBUG("Have retry after value %d", retry_after);
          conn_handle.set_return_to_pool(false);
          _resolver->blacklist(target, retry_after);
        }
        else
        {
          _resolver->success(target);
        }
      }
      else
      {
        _resolver->success(target);
      }

      // Determine the failure mode and update the correct counter.
      bool fatal_http_error = false;

      if (http_rc >= 400)
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
          fatal_http_error)
      {
        // Make a SAS log so that its clear that we have stopped retrying
        // deliberately.
        HttpErrorResponseTypes reason = fatal_http_error ?
                                        HttpErrorResponseTypes::Permanent :
                                        HttpErrorResponseTypes::Temporary;
        sas_log_http_abort(trail, reason, 0);
        break;
      }
    }
  }

  delete target_it;

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

  // Get the current time in ms
  struct timespec tp;
  int rv = clock_gettime(CLOCK_MONOTONIC, &tp);
  assert(rv == 0);
  unsigned long now_ms = tp.tv_sec * 1000 + (tp.tv_nsec / 1000000);

  if (rc == CURLE_OK)
  {
    if (_comm_monitor)
    {
      // If both attempts fail due to overloaded downstream nodes, consider
      // it a communication failure.
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
    if (_comm_monitor)
    {
      _comm_monitor->inform_failure(now_ms);
    }
  }

  if (((rc != CURLE_OK) && (rc != CURLE_REMOTE_FILE_NOT_FOUND)) || (http_code >= 400))
  {
    TRC_ERROR("cURL failure with cURL error code %d (see man 3 libcurl-errors) and HTTP error code %ld", (int)rc, http_code);  // LCOV_EXCL_LINE
  }

  return http_code;
}

struct curl_slist* HttpClient::build_headers(std::vector<std::string> headers_to_add,
                                             bool assert_user,
                                             const std::string& username,
                                             std::string uuid_str)
{
  struct curl_slist* extra_headers = NULL;
  extra_headers = curl_slist_append(extra_headers, "Content-Type: application/json");

  // Add the UUID for SAS correlation to the HTTP message.
  extra_headers = curl_slist_append(extra_headers,
                                    (SASEvent::HTTP_BRANCH_HEADER_NAME + ": " + uuid_str).c_str());

  // By default cURL will add `Expect: 100-continue` to certain requests. This
  // causes the HTTP stack to send 100 Continue responses, which messes up the
  // SAS call flow. To prevent this add an empty Expect header, which stops
  // cURL from adding its own.
  extra_headers = curl_slist_append(extra_headers, "Expect:");


  // Add in any extra headers
  for (std::vector<std::string>::const_iterator i = headers_to_add.begin();
       i != headers_to_add.end();
       ++i)
  {
    extra_headers = curl_slist_append(extra_headers, (*i).c_str());
  }

  // Add the user's identity (if required).
  if (assert_user)
  {
    extra_headers = curl_slist_append(extra_headers,
                                      ("X-XCAP-Asserted-Identity: " + username).c_str());
  }
  return extra_headers;
}

void HttpClient::set_curl_options_general(CURL* curl,
                                          std::string body,
                                          std::string& doc)
{
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &doc);

  if (!body.empty())
  {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  }
}

void HttpClient::set_curl_options_response(CURL* curl,
                               std::map<std::string, std::string>* response_headers)
{
  // If response_headers is not null, the headers returned by the curl request
  // should be stored there.
  if (response_headers)
  {
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &HttpClient::write_headers);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, response_headers);
  }
}

void HttpClient::set_curl_options_request(CURL* curl, RequestType request_type)
{
  switch (request_type)
  {
  case RequestType::DELETE:
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    break;
  case RequestType::PUT:
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    break;
  case RequestType::POST:
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    break;
  case RequestType::GET:
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
    break;
  }
}

/// cURL helper - write data into string.
size_t HttpClient::string_store(void* ptr, size_t size, size_t nmemb, void* stream)
{
  ((std::string*)stream)->append((char*)ptr, size * nmemb);
  return (size * nmemb);
}

size_t HttpClient::write_headers(void *ptr, size_t size, size_t nmemb, std::map<std::string, std::string> *headers)
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

  TRC_DEBUG("Received header %s with value %s", key.c_str(), val.c_str());
  TRC_DEBUG("Header pointer: %p", headers);
  (*headers)[key] = val;

  return size * nmemb;
}

void HttpClient::cleanup_uuid(void *uuid_gen)
{
  delete (RandomUUIDGenerator*)uuid_gen; uuid_gen = NULL;
}

boost::uuids::uuid HttpClient::get_random_uuid()
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

void HttpClient::sas_add_ip(SAS::Event& event, CURL* curl, CURLINFO info)
{
  char* ip;

  if (curl_easy_getinfo(curl, info, &ip) == CURLE_OK)
  {
    event.add_var_param(ip);
  }
  else
  {
    event.add_var_param("unknown"); // LCOV_EXCL_LINE FakeCurl cannot fail the getinfo call.
  }
}

void HttpClient::sas_add_port(SAS::Event& event, CURL* curl, CURLINFO info)
{
  long port;

  if (curl_easy_getinfo(curl, info, &port) == CURLE_OK)
  {
    event.add_static_param(port);
  }
  else
  {
    event.add_static_param(0); // LCOV_EXCL_LINE FakeCurl cannot fail the getinfo call.
  }
}

void HttpClient::sas_add_ip_addrs_and_ports(SAS::Event& event,
                                                CURL* curl)
{
  // Add the local IP and port.
  sas_add_ip(event, curl, CURLINFO_PRIMARY_IP);
  sas_add_port(event, curl, CURLINFO_PRIMARY_PORT);

  // Now add the remote IP and port.
  sas_add_ip(event, curl, CURLINFO_LOCAL_IP);
  sas_add_port(event, curl, CURLINFO_LOCAL_PORT);
}

std::string HttpClient::get_obscured_message_to_log(const std::string& message)
{
  std::string message_to_log;
  std::size_t body_pos = message.find(HEADERS_END);
  std::string headers = message.substr(0, body_pos);

  if (body_pos + 4 == message.length())
  {
    // No body, we can just log the request as normal.
    message_to_log = message;
  }
  else
  {
    message_to_log = headers + BODY_OMITTED;
  }

  return message_to_log;
}

void HttpClient::sas_log_http_req(SAS::TrailId trail,
                                  CURL* curl,
                                  const std::string& method_str,
                                  const std::string& url,
                                  const std::string& request_bytes,
                                  SAS::Timestamp timestamp,
                                  uint32_t instance_id)
{
  if (_sas_log_level != SASEvent::HttpLogLevel::NONE)
  {
    int event_id = ((_sas_log_level == SASEvent::HttpLogLevel::PROTOCOL) ?
                    SASEvent::TX_HTTP_REQ : SASEvent::TX_HTTP_REQ_DETAIL);
    SAS::Event event(trail, event_id, instance_id);

    sas_add_ip_addrs_and_ports(event, curl);

    if (!_should_omit_body)
    {
      event.add_compressed_param(request_bytes, &SASEvent::PROFILE_HTTP);
    }
    else
    {
      std::string message_to_log = get_obscured_message_to_log(request_bytes);
      event.add_compressed_param(message_to_log, &SASEvent::PROFILE_HTTP);
    }

    event.add_var_param(method_str);
    event.add_var_param(Utils::url_unescape(url));

    event.set_timestamp(timestamp);
    SAS::report_event(event);
  }
}

void HttpClient::sas_log_http_rsp(SAS::TrailId trail,
                                  CURL* curl,
                                  long http_rc,
                                  const std::string& method_str,
                                  const std::string& url,
                                  const std::string& response_bytes,
                                  uint32_t instance_id)
{
  if (_sas_log_level != SASEvent::HttpLogLevel::NONE)
  {
    int event_id = ((_sas_log_level == SASEvent::HttpLogLevel::PROTOCOL) ?
                    SASEvent::RX_HTTP_RSP : SASEvent::RX_HTTP_RSP_DETAIL);
    SAS::Event event(trail, event_id, instance_id);

    sas_add_ip_addrs_and_ports(event, curl);
    event.add_static_param(http_rc);

    if (!_should_omit_body)
    {
      event.add_compressed_param(response_bytes, &SASEvent::PROFILE_HTTP);
    }
    else
    {
      std::string message_to_log = get_obscured_message_to_log(response_bytes);
      event.add_compressed_param(message_to_log, &SASEvent::PROFILE_HTTP);
    }

    event.add_var_param(method_str);
    event.add_var_param(Utils::url_unescape(url));

    SAS::report_event(event);
  }
}

void HttpClient::sas_log_http_abort(SAS::TrailId trail,
                                    HttpErrorResponseTypes reason,
                                    uint32_t instance_id)
{
  int event_id = ((_sas_log_level == SASEvent::HttpLogLevel::PROTOCOL) ?
                    SASEvent::HTTP_ABORT : SASEvent::HTTP_ABORT_DETAIL);
  SAS::Event event(trail, event_id, instance_id);
  event.add_static_param(static_cast<uint32_t>(reason));
  SAS::report_event(event);
}

void HttpClient::sas_log_curl_error(SAS::TrailId trail,
                                    const char* remote_ip_addr,
                                    unsigned short remote_port,
                                    const std::string& method_str,
                                    const std::string& url,
                                    CURLcode code,
                                    uint32_t instance_id)
{
  if (_sas_log_level != SASEvent::HttpLogLevel::NONE)
  {
    int event_id = ((_sas_log_level == SASEvent::HttpLogLevel::PROTOCOL) ?
                    SASEvent::HTTP_REQ_ERROR : SASEvent::HTTP_REQ_ERROR_DETAIL);
    SAS::Event event(trail, event_id, instance_id);

    event.add_static_param(remote_port);
    event.add_static_param(code);
    event.add_var_param(remote_ip_addr);
    event.add_var_param(method_str);
    event.add_var_param(Utils::url_unescape(url));
    event.add_var_param(curl_easy_strerror(code));

    SAS::report_event(event);
  }
}

void HttpClient::sas_log_bad_retry_after_value(SAS::TrailId trail,
                                               const std::string value,
                                               uint32_t instance_id)
{
  if (_sas_log_level != SASEvent::HttpLogLevel::NONE)
  {
    int event_id = ((_sas_log_level == SASEvent::HttpLogLevel::PROTOCOL) ?
                    SASEvent::HTTP_BAD_RETRY_AFTER_VALUE : SASEvent::HTTP_BAD_RETRY_AFTER_VALUE_DETAIL);
    SAS::Event event(trail, event_id, instance_id);
    event.add_var_param(value.c_str());
    SAS::report_event(event);
  }
}

void HttpClient::host_port_from_server(const std::string& scheme,
                                       const std::string& server,
                                       std::string& host,
                                       int& port)
{
  std::string server_copy = server;
  Utils::trim(server_copy);
  size_t colon_idx;
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
    port = (scheme == "https") ? DEFAULT_HTTPS_PORT : DEFAULT_HTTP_PORT;
  }
}

std::string HttpClient::host_from_server(const std::string& scheme,
                                         const std::string& server)
{
  std::string host;
  int port;
  host_port_from_server(scheme, server, host, port);
  return host;
}

int HttpClient::port_from_server(const std::string& scheme,
                                 const std::string& server)
{
  std::string host;
  int port;
  host_port_from_server(scheme, server, host, port);
  return port;
}

HttpClient::Recorder::Recorder() {}

HttpClient::Recorder::~Recorder() {}

int HttpClient::Recorder::debug_callback(CURL *handle,
                                         curl_infotype type,
                                         char *data,
                                         size_t size,
                                         void *userptr)
{
  return ((Recorder*)userptr)->record_data(type, data, size);
}

int HttpClient::Recorder::record_data(curl_infotype type,
                                      char* data,
                                      size_t size)
{
  switch (type)
  {
  case CURLINFO_HEADER_IN:
  case CURLINFO_DATA_IN:
    response.append(data, size);
    break;

  case CURLINFO_HEADER_OUT:
  case CURLINFO_DATA_OUT:
    request.append(data, size);
    break;

  default:
    break;
  }

  return 0;
}
