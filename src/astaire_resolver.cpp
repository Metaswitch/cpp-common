/**
 * @file astaire_resolver.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "log.h"
#include "astaire_resolver.h"

static const uint16_t PORT = 11311;
static const int TRANSPORT = IPPROTO_TCP;


AstaireResolver::AstaireResolver(DnsCachedResolver* dns_client,
                                 int address_family,
                                 int blacklist_duration) :
  BaseResolver(dns_client),
  _address_family(address_family)
{
  // Create the blacklist.
  create_blacklist(blacklist_duration);
}


AstaireResolver::~AstaireResolver()
{
  destroy_blacklist();
}


void AstaireResolver::resolve(const std::string& host,
                              int max_targets,
                              std::vector<AddrInfo>& targets,
                              SAS::TrailId trail)
{
  std::string host_without_port;
  int port;
  AddrInfo ai;
  int dummy_ttl = 0;

  TRC_DEBUG("AstaireResolver::resolve for host %s, family %d",
            host.c_str(), _address_family);

  targets.clear();

  // Check if host contains a port. Otherwise use the default PORT.
  if (!Utils::split_host_port(host, host_without_port, port))
  {
    host_without_port = host;
    port = PORT;
  }

  if (parse_ip_target(host_without_port, ai.address))
  {
    // The name is already an IP address, so no DNS resolution is possible.
    TRC_DEBUG("Target is an IP address");
    ai.port = port;
    ai.transport = TRANSPORT;
    targets.push_back(ai);
  }
  else
  {
    a_resolve(host_without_port,
              _address_family,
              port,
              TRANSPORT,
              max_targets,
              targets,
              dummy_ttl,
              trail);
  }
}
