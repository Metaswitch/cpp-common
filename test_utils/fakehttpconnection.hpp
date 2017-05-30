/**
 * @file fakehttpconnection.h Fake HTTP connection for UT use.
 *
 * Copyright (C) Metaswitch Networks 2014
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

///
///

#pragma once

#include <map>
#include <string>

#include "httpconnection.h"

class FakeHttpConnection : public HttpConnection
{
public:
  FakeHttpConnection();
  virtual ~FakeHttpConnection();

  void flush_all();

  virtual long send_get(const std::string& uri, std::string& doc, const std::string& username, SAS::TrailId trail);
  bool put(const std::string& uri, const std::string& doc, const std::string& username, SAS::TrailId trail);
  bool del(const std::string& uri, const std::string& username, SAS::TrailId trail);

private:
  std::map<std::string, std::string> _db;
};

