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
                         std::string path) :
  _server(server),
  _scheme(scheme),
  _client(client),
  _path(path)
{
}

HttpRequest::~HttpRequest() {}

///
// SET methods
///
void HttpRequest::set_req_body(std::string body)
{
  _req_body = body;
}

void HttpRequest::set_req_headers(std::string req_header)
{
  _req_headers.push_back(req_header);
}

void HttpRequest::set_sas_trail(SAS::TrailId trail)
{
  _trail = trail;
}

void HttpRequest::set_allowed_host_state(int allowed_host_state)
{
  _allowed_host_state = allowed_host_state;
}

void HttpRequest::set_username(std::string username)
{
  _username = username;
}

///
// GET methods
///
HTTPCode HttpRequest::get_return_code()
{
  return _return_code;
}

std::string HttpRequest::get_recv_body()
{
  return _recv_body;
}

std::map<std::string, std::string> HttpRequest::get_recv_headers()
{
  return _recv_headers;
}

///
// Send requests
///
void HttpRequest::send(HttpClient::RequestType request_type)
{
  std::string url = _scheme + _server + _path;
  _return_code = _client->send_request(request_type,
                                      url,
                                      _req_body,
                                      _recv_body,
                                      _username,
                                      _trail,
                                      _req_headers,
                                      &_recv_headers,
                                      _allowed_host_state);
}
