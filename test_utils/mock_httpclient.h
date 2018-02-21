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
#include "httpclient.h"

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
};

#endif

