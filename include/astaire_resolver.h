/**
 * @file astaire_resolver.h
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef ASTAIRE_RESOLVER_H__
#define ASTAIRE_RESOLVER_H__

#include "baseresolver.h"
#include "sas.h"

class AstaireResolver : public BaseResolver
{
public:
  /// Constructor.
  ///
  /// @param dns_client         - Client to actually do the DNS lookup.
  /// @param address_family     - The address family (AF_INET/AF_INET6) to look
  ///                             up. Controls whether we do an A or AAAA
  ///                             lookup.
  /// @param blacklist_duration - The length of time that failed hosts should
  ///                             be blacklisted for.
  AstaireResolver(DnsCachedResolver* dns_client,
                  int address_family,
                  int blacklist_duration = DEFAULT_BLACKLIST_DURATION);

  /// Virtual destructor.
  virtual ~AstaireResolver();

  /// Resolve a domain representing an astaire cluster to a vector of targets
  /// in that domain.
  ///
  /// @param domain      - The domain name to resolve.
  /// @param max_targets - The maximum number of targets to return.
  /// @param targets     - (out) The returned targets.
  /// @param trail       - SAS trail ID.
  void resolve(const std::string& domain,
               int max_targets,
               std::vector<AddrInfo>& targets,
               SAS::TrailId trail);

  /// Default duration to blacklist hosts after we fail to connect to them.
  static const int DEFAULT_BLACKLIST_DURATION = 30;

private:
  int _address_family;
};

#endif

