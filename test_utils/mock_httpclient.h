/**
 * @file mock_httpclient.h
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_HTTPCLIENT_H__
#define MOCK_HTTPCLIENT_H__

#include "gmock/gmock.h"
#include "http_request.h"


// Various Matchers to help with matching an HttpRequest in the mock call to
// send_request()
bool http_method_matches(HttpRequest req, HttpClient::RequestType method);

MATCHER(IsDelete, "")
{
  return http_method_matches(arg, HttpClient::RequestType::DELETE);
}

MATCHER(IsPut, "")
{
  return http_method_matches(arg, HttpClient::RequestType::PUT);
}

MATCHER(IsPost, "")
{
  return http_method_matches(arg, HttpClient::RequestType::POST);
}

MATCHER(IsGet, "")
{
  return http_method_matches(arg, HttpClient::RequestType::GET);
}

MATCHER_P(HasScheme, scheme, "")
{
  return (arg._scheme == scheme);
}

MATCHER_P(HasServer, server, "")
{
  return (arg._server == server);
}

MATCHER_P(HasPath, path, "")
{
  return (arg._path == path);
}

MATCHER_P(HasBody, body, "")
{
  return (arg._body == body);
}

MATCHER_P(HasUsername, username, "")
{
  return (arg._username == username);
}

MATCHER_P(HasTrail, trail, "")
{
  return (arg._trail == trail);
}

MATCHER_P(HasHostState, host_state, "")
{
  return (arg._allowed_host_state == host_state);
}

MATCHER_P(HasHeader, header_string, "")
{
  const std::vector<std::string>& v = arg._headers;
  bool result = (std::find(v.begin(), v.end(), header_string) != v.end());
  return result;
}

class MockHttpClient : public HttpClient
{
public:
  MockHttpClient();
  virtual ~MockHttpClient();
  MOCK_METHOD9(send_request, long(RequestType request_type,
                                  const std::string& url,
                                  std::string body,
                                  std::string& response,
                                  const std::string& username,
                                  SAS::TrailId trail,
                                  std::vector<std::string> headers_to_add,
                                  std::map<std::string, std::string>* response_headers,
                                  int allowed_host_state));
  MOCK_METHOD1(send_request, HttpResponse(const HttpRequest&));
};

#endif

