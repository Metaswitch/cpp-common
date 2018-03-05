/**
 * @file httpclient.h Definitions for HttpClient class.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#pragma once

#include <map>

#include <curl/curl.h>
#include <sas.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include "utils.h"
#include "httpresolver.h"
#include "load_monitor.h"
#include "sasevent.h"
#include "communicationmonitor.h"
#include "snmp_ip_count_table.h"
#include "http_connection_pool.h"

typedef long HTTPCode;
static const long HTTP_OK = 200;
static const long HTTP_CREATED = 201;
static const long HTTP_ACCEPTED = 202;
static const long HTTP_NO_CONTENT = 204;
static const long HTTP_PARTIAL_CONTENT = 206;
static const long HTTP_BAD_REQUEST = 400;
static const long HTTP_UNAUTHORIZED = 401;
static const long HTTP_FORBIDDEN = 403;
static const long HTTP_NOT_FOUND = 404;
static const long HTTP_BADMETHOD = 405;
static const long HTTP_CONFLICT = 409;
static const long HTTP_PRECONDITION_FAILED = 412;
static const long HTTP_UNPROCESSABLE_ENTITY = 422;
static const long HTTP_TEMP_UNAVAILABLE = 480;
static const long HTTP_SERVER_ERROR = 500;
static const long HTTP_NOT_IMPLEMENTED = 501;
static const long HTTP_BAD_GATEWAY = 502;
static const long HTTP_SERVER_UNAVAILABLE = 503;
static const long HTTP_GATEWAY_TIMEOUT = 504;

static const std::string HEADERS_END = "\r\n\r\n";
static const std::string BODY_OMITTED = "\r\n\r\n<Body present but not logged>";

// We don't need to include http_request.h here, it can just be included in
// the .cpp file.
class HttpRequest;
class HttpResponse;

/// Issues HTTP requests, supporting round-robin DNS load balancing.
///
class HttpClient
{
public:
  // HttpConnectionPool requires access to the private Recorder class
  friend class HttpConnectionPool;
  // HttpRequest requires access to the private send_request function
  friend class HttpRequest;

  HttpClient(bool assert_user,
             HttpResolver* resolver,
             SNMP::IPCountTable* stat_table,
             LoadMonitor* load_monitor,
             SASEvent::HttpLogLevel sas_log_level,
             BaseCommunicationMonitor* comm_monitor,
             bool should_omit_body = false,
             bool remote_connection = false,
             long timeout_ms = -1,
             bool log_display_address = false,
             std::string server_display_address = "",
             const std::string& source_address = "");

  HttpClient(bool assert_user,
             HttpResolver* resolver,
             SASEvent::HttpLogLevel sas_log_level,
             BaseCommunicationMonitor* comm_monitor);

  virtual ~HttpClient();

  static size_t string_store(void* ptr, size_t size, size_t nmemb, void* stream);
  static void cleanup_curl(void* curlptr);
  static void cleanup_uuid(void* uuid_gen);

  /// Enum of HTTP request types, used when calling into send_request.
  enum struct RequestType {DELETE, PUT, POST, GET};

private:

  /// Class used to record HTTP transactions.
  class Recorder
  {
  public:
    Recorder();
    virtual ~Recorder();

    /// Static function that can be registered with a CURL handle (as the
    /// CURLOPT_DEBUGFUNCTION) to monitor all information it sends and receives.
    static int debug_callback(CURL *handle,
                              curl_infotype type,
                              char *data,
                              size_t size,
                              void *userptr);

    /// The recorded request data.
    std::string request;

    std::string response;

  private:
    /// Method that records information sent / received by a CURL handle in
    /// member variables.
    int record_data(curl_infotype type, char *data, size_t size);
  };

  static const int DEFAULT_HTTP_PORT = 80;
  static const int DEFAULT_HTTPS_PORT = 443;

  /// Converts RequestType to string for logging
  static std::string request_type_to_string(RequestType request_type);

  /// Sends the provided HTTP Request
  ///
  /// @param req  The HttpRequest to send
  ///
  /// @returns    The HttpResponse received.
  virtual HttpResponse send_request(const HttpRequest& req);

  /// Inner function to send an HTTP request.
  /// This is only a helper function, and should not be used directly. Instead,
  /// the send_request(const HttpRequest&) method should be used.
  ///
  /// @param request_type     The type of HTTP request to send
  /// @param url              Full URL to request - includes http(s)?://
  /// @param body             JSON body to send on the request
  /// @param response         Location to store retrieved data
  /// @param username         Username to assert if assertUser is true, else
  ///                         ignored
  /// @param trail            SAS trail to use
  /// @param headers_to_add   Extra headers to add to the request
  /// @param response_headers Location to store the header part of the retrieved
  ///                         data
  ///
  /// @returns                HTTP code representing outcome of request
  virtual long send_request(RequestType request_type,
                            const std::string& url,
                            std::string body,
                            std::string& response,
                            const std::string& username,
                            SAS::TrailId trail,
                            std::vector<std::string> headers_to_add,
                            std::map<std::string, std::string>* response_headers,
                            int allowed_host_state);

  /// Helper function that builds the curl header in the set_curl_options
  /// method.
  struct curl_slist* build_headers(std::vector<std::string> headers_to_add,
                                   bool has_body,
                                   bool assert_user,
                                   const std::string& username,
                                   std::string uuid_str);

  /// Helper function that sets the general curl options in send_request
  void set_curl_options_general(CURL* curl, std::string body, std::string& doc);

  /// Helper function that sets response header curl options, if required, in
  /// send_request
  void set_curl_options_response(CURL* curl,
                                 std::map<std::string, std::string>* response_headers);

  /// Helper function that sets request-type specific curl options in
  /// send_request
  void set_curl_options_request(CURL* curl, RequestType request_type);

  /// Helper functions that sets host-specific curl options in send_request
  virtual void* set_curl_options_host(CURL* curl, std::string host, int port)
  {
    return nullptr;
  }

  /// Clean-up function for any memory allocated by set_curl_options_host
  virtual void cleanup_host_context(void* host_context)
  {
    // Since nothing is created by set_curl_options_host above, there is nothing to
    // clean up in this function.
    (void) host_context;
  }

  void sas_add_ip(SAS::Event& event, CURL* curl, CURLINFO info);

  void sas_add_port(SAS::Event& event, CURL* curl, CURLINFO info);

  void sas_add_ip_addrs_and_ports(SAS::Event& event,
                                  CURL* curl);

  // Check if the message has a body and obscure it if so.
  std::string get_obscured_message_to_log(const std::string& message);

  void sas_log_http_req(SAS::TrailId trail,
                        CURL* curl,
                        const std::string& method_str,
                        const std::string& url,
                        const std::string& request_bytes,
                        SAS::Timestamp timestamp,
                        uint32_t instance_id);

  void sas_log_http_rsp(SAS::TrailId trail,
                        CURL* curl,
                        long http_rc,
                        const std::string& method_str,
                        const std::string& url,
                        const std::string& response_bytes,
                        uint32_t instance_id);

  void sas_log_curl_error(SAS::TrailId trail,
                          const char* remote_ip_addr,
                          unsigned short remote_port,
                          const std::string& method_str,
                          const std::string& url,
                          CURLcode code,
                          uint32_t instance_id,
                          const char* error);

  void sas_log_bad_retry_after_value(SAS::TrailId trail,
                                     const std::string value,
                                     uint32_t instance_id);

  /// Enum of response types to correspond with ENUM defined in SAS resource
  /// bundle. Make sure the two are kept in sync
  enum class HttpErrorResponseTypes : uint32_t
  {
    Temporary = 0,
    Permanent = 1
  };

  void sas_log_http_abort(SAS::TrailId trail,
                          HttpErrorResponseTypes reason,
                          uint32_t instance_id);

  HTTPCode curl_code_to_http_code(CURL* curl, CURLcode code);
  static size_t write_headers(void *ptr, size_t size, size_t nmemb, std::map<std::string, std::string> *headers);
  static void host_port_from_server(const std::string& scheme,
                                    const std::string& server,
                                    std::string& host,
                                    int& port);
  static std::string host_from_server(const std::string& scheme, const std::string& server);
  static int port_from_server(const std::string& scheme, const std::string& server);

  boost::uuids::uuid get_random_uuid();

  const bool _assert_user;
  pthread_key_t _uuid_thread_local;

  HttpResolver* _resolver;
  LoadMonitor* _load_monitor;
  pthread_mutex_t _lock;
  std::map<std::string, int> _server_count;  // must access under _lock
  SASEvent::HttpLogLevel _sas_log_level;
  BaseCommunicationMonitor* _comm_monitor;
  SNMP::IPCountTable* _stat_table;
  HttpConnectionPool _conn_pool;
  bool _should_omit_body;
  bool _log_display_address;
  std::string _server_display_address;
};
