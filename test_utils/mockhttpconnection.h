/**
 * @file mockhttpconnection.h Mock httpconnection.
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCKHTTPCONNECTION_H__
#define MOCKHTTPCONNECTION_H__

#include "gmock/gmock.h"
#include "httpconnection.h"

class MockHttpConnection : public HttpConnection
{
public:
  MockHttpConnection();
  ~MockHttpConnection();
  MOCK_METHOD5(send_post, long(const std::string& url_tail,
                               std::map<std::string, std::string>& headers,
                               const std::string& body,
                               SAS::TrailId trail,
                               const std::string& username));
  MOCK_METHOD4(send_get, long(const std::string& url_tail,
                              std::string& response,
                              const std::string& username,
                              SAS::TrailId trail));
};

#endif
