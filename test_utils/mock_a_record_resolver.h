/**
 * @file mock_a_record_resolver.h Mock ARecordResolver
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_A_RECORD_RESOLVER_H__
#define MOCK_A_RECORD_RESOLVER_H__

#include "gmock/gmock.h"
#include "a_record_resolver.h"

class MockARecordResolver : public ARecordResolver
{
public:
  MockARecordResolver() : ARecordResolver(nullptr, 0, 0, 0) {}
  ~MockARecordResolver() {}

  MOCK_METHOD3(resolve_iter, BaseAddrIterator*(const std::string& host,
                                               int port,
                                               SAS::TrailId trail));

  MOCK_METHOD5(resolve, void(const std::string& host,
                             int port,
                             int max_targets,
                             std::vector<AddrInfo>& targets,
                             SAS::TrailId trail));

  MOCK_METHOD1(blacklist, void(const AddrInfo& ai));
  MOCK_METHOD1(success, void(const AddrInfo& ai));
  MOCK_METHOD1(untested, void(const AddrInfo& ai));
};

typedef MockARecordResolver MockCassandraResolver;

#endif
