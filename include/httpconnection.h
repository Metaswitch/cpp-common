/**
 * @file httpconnection.h Definitions for HttpConnection class.
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

typedef long HTTPCode;
static const long HTTP_OK = 200;
static const long HTTP_CREATED = 201;
static const long HTTP_ACCEPTED = 202;
static const long HTTP_PARTIAL_CONTENT = 206;
static const long HTTP_BAD_REQUEST = 400;
static const long HTTP_UNAUTHORIZED = 401;
static const long HTTP_FORBIDDEN = 403;
static const long HTTP_NOT_FOUND = 404;
static const long HTTP_BADMETHOD = 405;
static const long HTTP_TEMP_UNAVAILABLE = 480;
static const long HTTP_SERVER_ERROR = 500;
static const long HTTP_NOT_IMPLEMENTED = 501;
static const long HTTP_SERVER_UNAVAILABLE = 503;
static const long HTTP_GATEWAY_TIMEOUT = 504;

/// Provides managed access to data on a single HTTP server. Properly
/// supports round-robin DNS load balancing.
///
class HttpConnection
{
public:
  HttpConnection(const std::string& server,
                 bool assert_user,
                 HttpResolver* resolver,
                 SNMP::IPCountTable* stat_table,
                 LoadMonitor* load_monitor,
                 SASEvent::HttpLogLevel,
                 CommunicationMonitor* comm_monitor);

  HttpConnection(const std::string& server,
                 bool assert_user,
                 HttpResolver* resolver,
                 SASEvent::HttpLogLevel,
                 CommunicationMonitor* comm_monitor);

  virtual ~HttpConnection();

  virtual long send_get(const std::string& path,
                        std::string& response,
                        std::vector<std::string> headers,
                        const std::string& override_server,
                        SAS::TrailId trail);
  virtual long send_get(const std::string& path,
                        std::string& response,
                        const std::string& username,
                        SAS::TrailId trail);
  virtual long send_get(const std::string& path,                     
                        std::map<std::string, std::string>& headers, 
                        std::string& response,                       
                        const std::string& username,                 
                        SAS::TrailId trail);                         
  virtual long send_get(const std::string& path,                     //< Absolute path to request from server - must start with "/"
                        std::map<std::string, std::string>& headers, //< Map of headers from the response
                        std::string& response,                       //< Retrieved document
                        const std::string& username,                 //< Username to assert (if assertUser was true, else ignored)
                        std::vector<std::string> headers_to_add,     //< Extra headers to add to the request
                        SAS::TrailId trail);                         //< SAS trail

  virtual long send_delete(const std::string& path,
                           SAS::TrailId trail,
                           const std::string& body,
                           const std::string& override_server);
  virtual long send_delete(const std::string& path,
                           SAS::TrailId trail,
                           const std::string& body = "");
  virtual long send_delete(const std::string& path,
                           SAS::TrailId trail,
                           const std::string& body,
                           std::string& response);
  virtual long send_delete(const std::string& path,                     //< Absolute path to request from server - must start with "/"
                           std::map<std::string, std::string>& headers, //< Map of headers from the response
                           std::string& response,                       //< Retrieved document
                           SAS::TrailId trail,                          //< SAS trail
                           const std::string& body = "",                //< Body to send in request
                           const std::string& username = "");           //< Username to assert (if assertUser was true, else ignored)

  virtual long send_put(const std::string& path,
                        const std::string& body,
                        SAS::TrailId trail,
                        const std::string& username = "");
  virtual long send_put(const std::string& path,
                        std::string& response,
                        const std::string& body,
                        SAS::TrailId trail,
                        const std::string& username = "");
  virtual long send_put(const std::string& path,
                        std::map<std::string, std::string>& headers,
                        const std::string& body,
                        SAS::TrailId trail,
                        const std::string& username = "");
  virtual long send_put(const std::string& path,                     //< Absolute path to request from server - must start with "/"
                        std::map<std::string, std::string>& headers, //< Map of headers from the response
                        std::string& response,                       //< Retrieved document
                        const std::string& body,                     //< Body to send in request
                        SAS::TrailId trail,                          //< SAS trail
                        const std::string& username = "");           //< Username to assert (if assertUser was true, else ignored)

  virtual long send_post(const std::string& path,
                         std::map<std::string, std::string>& headers,
                         const std::string& body,
                         SAS::TrailId trail,
                         const std::string& username = "");
  virtual long send_post(const std::string& path,                     //< Absolute path to request from server - must start with "/"
                         std::map<std::string, std::string>& headers, //< Map of headers from the response
                         std::string& response,                       //< Retrieved document
                         const std::string& body,                     //< Body to send in request
                         SAS::TrailId trail,                          //< SAS trail
                         const std::string& username = "");           //< Username to assert (if assertUser was true, else ignored)

  virtual long send_request(const std::string& path,
                            std::string body,
                            std::string& doc,
                            const std::string& username,
                            SAS::TrailId trail,
                            const std::string& method_str,
                            std::vector<std::string> headers,
                            CURL* curl);

  static size_t string_store(void* ptr, size_t size, size_t nmemb, void* stream);
  static void cleanup_curl(void* curlptr);
  static void cleanup_uuid(void* uuid_gen);

private:

  /// A single entry in the connection pool. Stored inside a cURL handle.
  class PoolEntry
  {
  public:
    PoolEntry(HttpConnection* parent);
    ~PoolEntry();

    void set_remote_ip(const std::string& value);
    const std::string& get_remote_ip() const { return _remote_ip; };

    bool is_connection_expired(unsigned long now_ms);
    void update_deadline(unsigned long now_ms);

  private:
    void update_snmp_ip_counts(const std::string& value);

    /// Parent HttpConnection object.
    HttpConnection* _parent;

    /// Time beyond which this connection should be recycled, in
    // CLOCK_MONOTONIC milliseconds, or 0 for ASAP.
    unsigned long _deadline_ms;

    /// Random distribution to use for determining connection lifetimes.
    /// Use an exponential distribution because it is memoryless. This
    /// gives us a Poisson distribution of recycle events, both for
    /// individual threads and for the overall application.
    Utils::ExponentialDistribution _rand;

    /// Server IP we're connected to, if any.
    std::string _remote_ip;
  };

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


  void sas_add_ip(SAS::Event& event, CURL* curl, CURLINFO info);

  void sas_add_port(SAS::Event& event, CURL* curl, CURLINFO info);

  void sas_add_ip_addrs_and_ports(SAS::Event& event,
                                  CURL* curl);

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
                          uint32_t instance_id);

  CURL* get_curl_handle();
  void reset_curl_handle(CURL* curl);
  HTTPCode curl_code_to_http_code(CURL* curl, CURLcode code);
  static size_t write_headers(void *ptr, size_t size, size_t nmemb, std::map<std::string, std::string> *headers);
  static void host_port_from_server(const std::string& server, std::string& host, int& port);
  static std::string host_from_server(const std::string& server);
  static int port_from_server(const std::string& server);
  static long calc_req_timeout_from_latency(int latency_us);
  void change_server(std::string override_server);

  boost::uuids::uuid get_random_uuid();

  std::string _server;
  std::string _host;
  int _port;
  const bool _assert_user;
  pthread_key_t _curl_thread_local;
  pthread_key_t _uuid_thread_local;

  HttpResolver* _resolver;
  LoadMonitor* _load_monitor;
  long _timeout_ms;
  pthread_mutex_t _lock;
  std::map<std::string, int> _server_count;  // must access under _lock
  SASEvent::HttpLogLevel _sas_log_level;
  CommunicationMonitor* _comm_monitor;
  SNMP::IPCountTable* _stat_table;

  friend class PoolEntry; // so it can update stats
};

