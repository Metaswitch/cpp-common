/**
 * @file mockhttpresolver.h Mock HttpResolver
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCKHTTPRESOLVER_H__
#define MOCKHTTPRESOLVER_H__

#include "gmock/gmock.h"
#include "httpresolver.h"

class MockHttpResolver : public HttpResolver
{
public:
  MockHttpResolver() : HttpResolver(nullptr, 0, 0, 0) {}
  ~MockHttpResolver() {}

  MOCK_METHOD4(resolve_iter, BaseAddrIterator*(const std::string& host,
                                               int port,
                                               SAS::TrailId trail,
                                               int allowed_host_state));
  MOCK_METHOD6(resolve, void(const std::string& host,
                             int port,
                             int max_targets,
                             std::vector<AddrInfo>& targets,
                             SAS::TrailId trail,
                             int allowed_host_state));

  MOCK_METHOD1(blacklist, void(const AddrInfo& ai));
  MOCK_METHOD2(blacklist, void(const AddrInfo& ai, int blacklist_ttl));
  MOCK_METHOD1(success, void(const AddrInfo& ai));
  MOCK_METHOD1(untested, void(const AddrInfo& ai));
};

#endif
