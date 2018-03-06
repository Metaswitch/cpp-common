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
HttpRequest::~HttpRequest() {}

///
// SET methods
///
HttpRequest& HttpRequest::set_body(const std::string& body)
{
  _body = body;
  return *this;
}

HttpRequest& HttpRequest::set_sas_trail(SAS::TrailId trail)
{
  _trail = trail;
  return *this;
}

HttpRequest& HttpRequest::set_allowed_host_state(int allowed_host_state)
{
  _allowed_host_state = allowed_host_state;
  return *this;
}

HttpRequest& HttpRequest::set_username(const std::string& username)
{
  _username = username;
  return *this;
}

///
// ADD methods
///
HttpRequest& HttpRequest::add_header(const std::string& header)
{
  _headers.push_back(header);
  return *this;
}

///
// Send requests
///
HttpResponse HttpRequest::send()
{
  return _client->send_request(*this);
}

///
// HTTP Response Object
///
HttpResponse::HttpResponse(
                HTTPCode return_code,
                const std::string& body,
                const std::map<std::string, std::string>& headers) :
    _rc(return_code),
    _body(body),
    _headers(headers)
    {}

HttpResponse::~HttpResponse() {}

///
// GET methods
///
HTTPCode HttpResponse::get_rc()
{
  return _rc;
}

std::string HttpResponse::get_body()
{
  return _body;
}

std::map<std::string, std::string> HttpResponse::get_headers()
{
  return _headers;
}
