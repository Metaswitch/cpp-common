/**
 * @file http_request.h Definitions for HttpRequest class.
 *
 * Copyright (C) Metaswitch Networks 2018
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


class HttpRequestBuilder
{
public:

  HttpRequestBuilder(std::string server,
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
                     std::string server_display_address = "");

  virtual ~HttpRequestBuilder();

  class HttpRequest
  {
  public:
    HttpRequest();
    virtual ~HttpRequest();
 
    // ADD method adds headers to any existing defaults, overwriting any duplicate entries??
    virtual void add_req_headers(std::vector<std::string> req_headers);

    // SET methods will overwrite any previous settings
    virtual void set_req_url(std::string);
    virtual void set_req_server(std::string); // This will be inherited from the builder, but do we ever want to override?
    virtual void set_req_body(std::string body);
    virtual void set_sas_trail(SAS::TrailId trail);
    virtual void set_allowed_host_state(int allowed_host_state);
    virtual void set_username(std::string username); //Unclear if we ever actually use this atm, may be unnecessary

    // Returns the HTTPCode, and populates recv headers and body
    virtual long send(RequestType request_type);

    // GET methods return NULL if empty/not set yet
    virtual std::map<std::string, std::string> get_recv_headers();
    virtual std::string get_recv_body();

  private:
    // member variables for storing the request information pre and post send
    std::string server;
    std::string req_url_tail;
    std::string req_body;
    std::vector<std::string> req_headers;
    std::string recv_body;
    std::map<std::string, std::string> recv_headers;
    SAS::TrailId trail;
    int allowed_host_state;
  };

private:
  std::string server;
  HttpResolver* resolver;
  LoadMonitor* load_monitor;
  HttpConnectionPool _conn_pool;
  SNMP::IPCountTable* stat_table;
  std::string _scheme
}
