/**
 * @file a_record_resolver.cpp  Implementation of A record DNS resolver class.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014 Metaswitch Networks Ltd
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
#include "a_record_resolver.h"

ARecordResolver::ARecordResolver(DnsCachedResolver* dns_client,
                                 int address_family,
                                 int blacklist_duration,
                                 int graylist_duration,
                                 const int default_port) :
  BaseResolver(dns_client),
  _address_family(address_family),
  _default_port(default_port)
{
  TRC_DEBUG("Creating ARecordResolver");

  // Create the blacklist.
  create_blacklist(blacklist_duration, graylist_duration);

  TRC_STATUS("Created ARecordResolver");
}

ARecordResolver::~ARecordResolver()
{
  destroy_blacklist();
}

void ARecordResolver::resolve(const std::string& host,
                              int port,
                              int max_targets,
                              std::vector<AddrInfo>& targets,
                              SAS::TrailId trail)
{
  BaseAddrIterator* addr_it = resolve_iter(host, port, trail);
  targets = addr_it->take(max_targets);
  delete addr_it; addr_it = nullptr;
}

BaseAddrIterator* ARecordResolver::resolve_iter(const std::string& host,
                                                int port,
                                                SAS::TrailId trail)
{
  BaseAddrIterator* addr_it;

  TRC_DEBUG("ARecordResolver::resolve_iter for host %s, port %d, family %d",
            host.c_str(), port, _address_family);

  port = (port != 0) ? port : _default_port;
  AddrInfo ai;

  if (parse_ip_target(host, ai.address))
  {
    // The name is already an IP address so no DNS resolution is possible.
    TRC_DEBUG("Target is an IP address");
    ai.port = port;
    ai.transport = TRANSPORT;
    addr_it = new SimpleAddrIterator(std::vector<AddrInfo>(1, ai));
  }
  else
  {
    int dummy_ttl = 0;
    addr_it = a_resolve_iter(host, _address_family, port, TRANSPORT, dummy_ttl, trail);
  }

  return addr_it;
}
