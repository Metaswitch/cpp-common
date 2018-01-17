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

  MOCK_METHOD2(create_request, HttpRequest(HttpClient::RequestType method, std::string path));
};

#endif
