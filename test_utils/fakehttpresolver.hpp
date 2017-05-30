/**
 * @file fakehttpresolver.hpp Fake HttpResolver for testing
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef FAKEHTTPRESOLVER_H__
#define FAKEHTTPRESOLVER_H__

#include "httpresolver.h"

typedef SimpleAddrIterator FakeAddrIterator;

class FakeHttpResolver : public HttpResolver
{
public:
  FakeHttpResolver() : HttpResolver(nullptr, AF_INET) {}

  FakeHttpResolver(const std::string& ip) : HttpResolver(nullptr, AF_INET)
  {
    _targets.push_back(FakeHttpResolver::create_target(ip));
  }

  FakeHttpResolver(const std::string& ip1, const std::string& ip2) :
    HttpResolver(nullptr, 0)
  {
    _targets.push_back(FakeHttpResolver::create_target(ip1));
    _targets.push_back(FakeHttpResolver::create_target(ip2));
  }

  ~FakeHttpResolver() {}

  virtual BaseAddrIterator* resolve_iter(const std::string& host,
                                         int port,
                                         SAS::TrailId trail)
  {
    std::vector<AddrInfo> targets = _targets;

    // Set the port on the targets to be returned
    for (std::vector<AddrInfo>::iterator it = targets.begin();
         it != targets.end();
         ++it)
    {
      it->port = (port != 0) ? port : 80;
    }

    return new FakeAddrIterator(targets);
  }

  virtual void success(const AddrInfo& ai) {};
  virtual void blacklist(const AddrInfo& ai) {};
  virtual void untested(const AddrInfo& ai) {};

  std::vector<AddrInfo> _targets;

  /// Creates a single AddrInfo target from the given IP address string, and
  /// optional port.
  static AddrInfo create_target(std::string address_str, int port = 80)
  {
    AddrInfo ai;
    BaseResolver::parse_ip_target(address_str, ai.address);
    ai.port = port;
    ai.transport = IPPROTO_TCP;
    return ai;
  }

  /// Creates a vector of count AddrInfo targets, beginning from 3.0.0.0 and
  /// incrementing by one each time.
  static std::vector<AddrInfo> create_targets(int count)
  {
    std::vector<AddrInfo> targets;
    std::stringstream os;
    for (int i = 0; i < count; ++i)
    {
      os << "3.0.0." << i;
      targets.push_back(create_target(os.str()));
      os.str(std::string());
    }
    return targets;
  }
};

#endif
