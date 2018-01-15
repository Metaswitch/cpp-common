/**
 * @file http_request.cpp HttpRequest class methods.
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "http_request.h"

/// Create an HTTP Request builder object.
///
/// @param server Server to send requests to
/// @param assert_user Assert user in header?
/// @param resolver HTTP resolver to use to resolve server's IP addresses
/// @param stat_name SNMP table to report connection info to.
/// @param load_monitor Load Monitor.
/// @param sas_log_level the level to log HTTP flows at (none/protocol/detail).
/// @param log_display_address Log an address other than server to SAS?
/// @param server_display_address The address to log in the SAS call flow
HttpRequestBuilder::HttpRequestBuilder(const std::string server,
                                       bool assert_user,
                                       HttpResolver* resolver,
                                       SNMP::IPCountTable* stat_table,
                                       LoadMonitor* load_monitor,
                                       SASEvent::HttpLogLevel sas_log_level,
                                       BaseCommunicationMonitor* comm_monitor,
                                       const std::string& scheme = "http",
                                       bool should_omit_body = false,
                                       bool remote_connection = false,
                                       long timeout_ms = -1,
                                       bool log_display_address = false,
                                       std::string server_display_address = "") :
  _scheme(scheme),
  _server(server),
  _assert_user(assert_user),
  _resolver(resolver),
  _load_monitor(load_monitor),
  _sas_log_level(sas_log_level),
  _comm_monitor(comm_monitor),
  _stat_table(stat_table),
  _conn_pool(load_monitor, stat_table, remote_connection, timeout_ms),
  _should_omit_body(should_omit_body),
  _log_display_address(log_display_address),
  _server_display_address(server_display_address),
  _default_allowed_host_state(BaseResolver::ALL_LISTS)
{
  pthread_key_create(&_uuid_thread_local, cleanup_uuid);
  pthread_mutex_init(&_lock, NULL);
  curl_global_init(CURL_GLOBAL_DEFAULT);

  TRC_STATUS("Configuring HTTP Connection");
  TRC_STATUS("  Connection created for server %s", _server.c_str());
}

//TODO other constructor variations

HttpRequestBuilder::~HttpRequestBuilder()
{
//TODO what does this tie into
  RandomUUIDGenerator* uuid_gen =
    (RandomUUIDGenerator*)pthread_getspecific(_uuid_thread_local);

  if (uuid_gen != NULL)
  {
    pthread_setspecific(_uuid_thread_local, NULL);
    cleanup_uuid(uuid_gen); uuid_gen = NULL;
  }

  pthread_key_delete(_uuid_thread_local);
}


// HTTP Request object
HttpRequestBuilder::HttpRequest::HttpRequest(int default_allowed_host_state)
{
  _allowed_host_state(default_allowed_host_state)
}

HttpRequestBuilder::HttpRequest::~HttpRequest() {}


// Map the CURLcode into a sensible HTTP return code.
HTTPCode HttpRequestBuilder::curl_code_to_http_code(CURL* curl, CURLcode code)
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


// Utilities
struct curl_slist* HttpRequestBuilder::build_headers(std::vector<std::string> headers_to_add,
                                             bool has_body,
                                             bool assert_user,
                                             const std::string& username,
                                             std::string uuid_str)
{
  struct curl_slist* extra_headers = NULL;

  if (has_body)
  {
    extra_headers = curl_slist_append(extra_headers, "Content-Type: application/json");
  }

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

void HttpRequestBuilder::set_curl_options_general(CURL* curl,
                                          std::string body,
                                          std::string& doc)
{
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &doc);

  if (!body.empty())
  {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  }
}

void HttpRequestBuilder::set_curl_options_response(CURL* curl,
                               std::map<std::string, std::string>* response_headers)
{
  // If response_headers is not null, the headers returned by the curl request
  // should be stored there.
  if (response_headers)
  {
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &HttpRequestBuilder::write_headers);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, response_headers);
  }
}

void HttpRequestBuilder::set_curl_options_request(CURL* curl, RequestType request_type)
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
size_t HttpRequestBuilder::string_store(void* ptr, size_t size, size_t nmemb, void* stream)
{
  ((std::string*)stream)->append((char*)ptr, size * nmemb);
  return (size * nmemb);
}

size_t HttpRequestBuilder::write_headers(void *ptr, size_t size, size_t nmemb, std::map<std::string, std::string> *headers)
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

void HttpRequestBuilder::cleanup_uuid(void *uuid_gen)
{
  delete (RandomUUIDGenerator*)uuid_gen; uuid_gen = NULL;
}

boost::uuids::uuid HttpRequestBuilder::get_random_uuid()
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

void HttpRequestBuilder::sas_add_ip(SAS::Event& event, CURL* curl, CURLINFO info)
{
  char* ip;

  if (curl_easy_getinfo(curl, info, &ip) == CURLE_OK)
  {
    if ((_log_display_address) && (info == CURLINFO_PRIMARY_IP))
    {
      // The HttpRequestBuilder is configured to log an address other than the server
      event.add_var_param(_server_display_address);
    }
    else
    {
      event.add_var_param(ip);
    }
  }
  else
  {
    event.add_var_param("unknown"); // LCOV_EXCL_LINE FakeCurl cannot fail the getinfo call.
  }
}

void HttpRequestBuilder::sas_add_port(SAS::Event& event, CURL* curl, CURLINFO info)
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

void HttpRequestBuilder::sas_add_ip_addrs_and_ports(SAS::Event& event,
                                                CURL* curl)
{
  // Add the remote IP and port.
  sas_add_ip(event, curl, CURLINFO_PRIMARY_IP);
  sas_add_port(event, curl, CURLINFO_PRIMARY_PORT);

  // Now add the local IP and port.
  sas_add_ip(event, curl, CURLINFO_LOCAL_IP);
  sas_add_port(event, curl, CURLINFO_LOCAL_PORT);
}

std::string HttpRequestBuilder::get_obscured_message_to_log(const std::string& message)
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

void HttpRequestBuilder::sas_log_http_req(SAS::TrailId trail,
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

void HttpRequestBuilder::sas_log_http_rsp(SAS::TrailId trail,
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

void HttpRequestBuilder::sas_log_http_abort(SAS::TrailId trail,
                                    HttpErrorResponseTypes reason,
                                    uint32_t instance_id)
{
  int event_id = ((_sas_log_level == SASEvent::HttpLogLevel::PROTOCOL) ?
                    SASEvent::HTTP_ABORT : SASEvent::HTTP_ABORT_DETAIL);
  SAS::Event event(trail, event_id, instance_id);
  event.add_static_param(static_cast<uint32_t>(reason));
  SAS::report_event(event);
}

void HttpRequestBuilder::sas_log_curl_error(SAS::TrailId trail,
                                    const char* remote_ip_addr,
                                    unsigned short remote_port,
                                    const std::string& method_str,
                                    const std::string& url,
                                    CURLcode code,
                                    uint32_t instance_id,
                                    const char* error)
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
    event.add_var_param(error);

    SAS::report_event(event);
  }
}

void HttpRequestBuilder::sas_log_bad_retry_after_value(SAS::TrailId trail,
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

void HttpRequestBuilder::host_port_from_server(const std::string& scheme,
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

std::string HttpRequestBuilder::host_from_server(const std::string& scheme,
                                         const std::string& server)
{
  std::string host;
  int port;
  host_port_from_server(scheme, server, host, port);
  return host;
}

int HttpRequestBuilder::port_from_server(const std::string& scheme,
                                 const std::string& server)
{
  std::string host;
  int port;
  host_port_from_server(scheme, server, host, port);
  return port;
}

HttpRequestBuilder::Recorder::Recorder() {}

HttpRequestBuilder::Recorder::~Recorder() {}

int HttpRequestBuilder::Recorder::debug_callback(CURL *handle,
                                         curl_infotype type,
                                         char *data,
                                         size_t size,
                                         void *userptr)
{
  return ((Recorder*)userptr)->record_data(type, data, size);
}

int HttpRequestBuilder::Recorder::record_data(curl_infotype type,
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
