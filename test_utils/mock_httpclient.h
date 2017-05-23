/**
 * @file mock_httpclient.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_HTTPCLIENT_H__
#define MOCK_HTTPCLIENT_H__

#include "httpclient.h"

class MockHttpClient : public HttpClient
{
  MockHttpClient() :
    HttpClient(false,
               nullptr,
               SASEvent::HttpLogLevel::PROTOCOL,
               nullptr)
  {}

  MOCK_METHOD5(send_post, long(const std::string& url,
                               std::map<std::string, std::string>& headers,
                               const std::string& body,
                               SAS::TrailId trail,
                               const std::string& username));

  // Add more mock methods as and when they are required.
};

#endif

