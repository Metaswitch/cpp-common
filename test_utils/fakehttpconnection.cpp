/**
 * @file fakehttpconnection.cpp Fake HTTP connection (for testing).
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */


#include <cstdio>
#include "fakehttpconnection.hpp"

using namespace std;

FakeHttpConnection::FakeHttpConnection() :
  // Initialize with dummy values.
  HttpConnection("localhost",
                 true,
                 NULL,
                 SASEvent::HttpLogLevel::PROTOCOL,
                 NULL)
{
}

FakeHttpConnection::~FakeHttpConnection()
{
  flush_all();
}

void FakeHttpConnection::flush_all()
{
  _db.clear();
}

long FakeHttpConnection::send_get(const std::string& uri, std::string& doc, const std::string& username, SAS::TrailId trail)
{
  std::map<std::string, std::string>::iterator i = _db.find(uri);
  if (i != _db.end())
  {
    doc = i->second;
    return 200;
  }
  return 404;
}

bool FakeHttpConnection::put(const std::string& uri, const std::string& doc, const std::string& username, SAS::TrailId trail)
{
  _db[uri] = doc;
  return true;
}

bool FakeHttpConnection::del(const std::string& uri, const std::string& username, SAS::TrailId trail)
{
  _db.erase(uri);
  return true;
}
