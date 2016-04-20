/**
 * @file astaire_resolver.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2015  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
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
