/**
 * @file static_dns_cache.h Definitions for the static DNS cache.
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef STATICDNSCACHE_H__
#define STATICDNSCACHE_H__

#include <string>
#include <map>
#include <vector>

#include "dnsrrecords.h"

class StaticDnsCache
{
public:
  StaticDnsCache(const std::string filename = "");
  ~StaticDnsCache();

  // Parse the _dns_config_file.
  void reload_static_records();

  // Returns the number of records in the static cache.
  int size() {return _static_records.size();};

  // Returns all DNS records from _static_records that match the given
  // domain/type combination (_static_records are parsed from the
  // _dns_config_file).
  DnsResult get_static_dns_records(std::string domain, int dns_type);

  // Resolves a CNAME record and returns the associated canonical domain.
  std::string get_canonical_name(std::string domain);

private:
  std::string _dns_config_file;
  std::map<std::string, std::vector<DnsRRecord*>> _static_records;
};

#endif
