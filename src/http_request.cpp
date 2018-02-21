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

#include "httpclient.h"
#include "http_request.h"

/// Create an HTTP Request builder object.
///
/// @param server Server to send requests to
/// @param scheme Scheme of the request being sent
/// @param client HttpClient object to use in sending the requst
/// @param path   Path the request should be sent to, starting with '/'
HttpRequest::HttpRequest(const std::string& server,
                         const std::string& scheme,
                         HttpClient* client,
                         HttpClient::RequestType method,
                         std::string& path) :
  _server(server),
  _scheme(scheme),
  _client(client),
  _method(method),
  _path(path)
{
}

HttpRequest::~HttpRequest() {}

///
// SET methods
///
void HttpRequest::set_req_body(const std::string& body)
{
  _req_body = body;
}

void HttpRequest::set_sas_trail(SAS::TrailId trail)
{
  _trail = trail;
}

void HttpRequest::set_allowed_host_state(int allowed_host_state)
{
  _allowed_host_state = allowed_host_state;
}

void HttpRequest::set_username(const std::string& username)
{
  _username = username;
}

///
// ADD methods
///
void HttpRequest::add_req_header(const std::string& req_header)
{
  _req_headers.push_back(req_header);
}

///
// Send requests
///
HttpResponse HttpRequest::send()
{
  std::string url = _scheme + "://" + _server + _path;

  std::string resp_body;
  std::map<std::string, std::string> resp_headers;

  HTTPCode rc = _client->send_request(_method,
                                      url,
                                      _req_body,
                                      resp_body,
                                      _username,
                                      _trail,
                                      _req_headers,
                                      &resp_headers,
                                      _allowed_host_state);

  return HttpResponse(rc,
                      resp_body,
                      resp_headers);
}

///
// HTTP Response Object
///
HttpResponse::HttpResponse(
                HTTPCode return_code,
                const std::string& resp_body,
                const std::map<std::string, std::string>& resp_headers) :
    _return_code(return_code),
    _resp_body(resp_body),
    _resp_headers(resp_headers)
    {}

HttpResponse::~HttpResponse() {}

///
// GET methods
///
HTTPCode HttpResponse::get_return_code()
{
  return _return_code;
}

std::string HttpResponse::get_resp_body()
{
  return _resp_body;
}

std::map<std::string, std::string> HttpResponse::get_resp_headers()
{
  return _resp_headers;
}
