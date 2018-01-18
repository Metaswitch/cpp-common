/**
 * @file mock_http_request.h
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_HTTP_REQUEST_H__
#define MOCK_HTTP_REQUEST_H__

#include "http_request.h"
#include "gmock/gmock.h"

class MockHttpRequest : public HttpRequest
{
public:
  MockHttpRequest();
  ~MockHttpRequest();

  // SET methods
  MOCK_METHOD1(set_req_body, void(std::string body));
  MOCK_METHOD1(add_req_header, void(std::string req_header));
  MOCK_METHOD1(set_sas_trail, void(SAS::TrailId trail));
  MOCK_METHOD1(set_allowed_host_state, void(int allowed_host_state));
  MOCK_METHOD1(set_username, void(std::string username));

  MOCK_METHOD0(send, HttpResponse());
};

#endif


