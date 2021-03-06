/**
 * @file mock_httpclient.cpp Mock httpclient.
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "mock_httpclient.h"
#include "http_request.h"

bool http_method_matches(HttpRequest req, HttpClient::RequestType method)
{
  return (req._method == method);
}

MockHttpClient::MockHttpClient() :
    HttpClient(false,
               nullptr,
               SASEvent::HttpLogLevel::PROTOCOL,
               nullptr)
  {}

MockHttpClient::~MockHttpClient() {}
