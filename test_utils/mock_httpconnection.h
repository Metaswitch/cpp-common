/**
 * @file mock_httpconnection.h Mock httpconnection.
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_HTTPCONNECTION_H__
#define MOCK_HTTPCONNECTION_H__

#include "gmock/gmock.h"
#include "httpconnection.h"

class MockHttpConnection : public HttpConnection
{
public:
  MockHttpConnection();
  ~MockHttpConnection();

  // GMock requires return values of mocked methods to be copyable, which
  // std::unique_ptr is not.
  // The workaround is to proxy the mocked method, and have the create_request
  // method be a real method that returns the std::unique_ptr to whatever the
  // mock method (create_request_proxy) is set to return.
  virtual std::unique_ptr<HttpRequest> create_request(HttpClient::RequestType method, std::string path) override
  {
    std::unique_ptr<HttpRequest> req(create_request_proxy(method, path));
    return req;
  };

  MOCK_METHOD2(create_request_proxy, HttpRequest*(HttpClient::RequestType method, std::string path));
};

#endif
