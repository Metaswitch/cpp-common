/**
 * @file a_record_resolver.h  Declaration of A record DNS resolver class.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef A_RECORD_RESOLVER_H_
#define A_RECORD_RESOLVER_H_

#include "baseresolver.h"
#include "sas.h"

class ARecordResolver : public BaseResolver
{
public:
  ARecordResolver(DnsCachedResolver* dns_client,
                  int address_family,
                  int blacklist_duration = DEFAULT_BLACKLIST_DURATION,
                  int graylist_duration = DEFAULT_GRAYLIST_DURATION,
                  const int default_port = 0);
  ~ARecordResolver();

  // Resolve a host name to a list of AddrInfo targets using an A record lookup.
  virtual void resolve(const std::string& host,
                       int port,
                       int max_targets,
                       std::vector<AddrInfo>& targets,
                       SAS::TrailId trail);

  // Lazily resolve a hostname to a list of AddrInfo targets using an A record
  // lookup.
  virtual BaseAddrIterator* resolve_iter(const std::string& host,
                                         int port,
                                         SAS::TrailId trail);

  /// Default duration to blacklist hosts after we fail to connect to them.
  static const int DEFAULT_BLACKLIST_DURATION = 30;
  static const int DEFAULT_GRAYLIST_DURATION = 30;

  static const int TRANSPORT = IPPROTO_TCP;

private:
  int _address_family;
  const int _default_port;
};

typedef ARecordResolver CassandraResolver;

#endif
