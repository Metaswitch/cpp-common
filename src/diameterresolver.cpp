/**
 * @file diameterresolver.cpp  Implementation of Diameter DNS resolver class.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
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
#include "diameterresolver.h"
#include "sas.h"
#include "sasevent.h"

DiameterResolver::DiameterResolver(DnsCachedResolver* dns_client,
                                   int af) :
  BaseResolver(dns_client),
  _af(af)
{
  LOG_DEBUG("Creating Diameter resolver");

  // Create the NAPTR cache.
  std::map<std::string, int> naptr_services;
  naptr_services["AAA+D2T"] = IPPROTO_TCP;
  create_naptr_cache(naptr_services);

  // Create the SRV cache.
  create_srv_cache();

  // Create the blacklist.
  create_blacklist();

  LOG_STATUS("Created Diameter resolver");
}

DiameterResolver::~DiameterResolver()
{
  destroy_blacklist();
  destroy_srv_cache();
  destroy_naptr_cache();
}

void DiameterResolver::resolve(const std::string& name,
                               int max_peers,
                               std::vector<AddrInfo>& targets)
{
  targets.clear();

  LOG_DEBUG("DiameterResolver::resolve for name %s, family %d",
            name.c_str(), _af);

  std::string srv_name;
  std::string a_name = name;

  NAPTRReplacement* naptr = _naptr_cache->get(name);

  if ((naptr != NULL) && (naptr->transport == IPPROTO_TCP))
  {
    // NAPTR resolved to a supported service and transport. We currently
    // only support TCP.
    LOG_DEBUG("NAPTR resolved to a supported service and transport");

    if (naptr->flags == "S")
    {
      // Do an SRV lookup with the replacement domain from the NAPTR lookup.
      srv_name = naptr->replacement;
    }
    else
    {
      a_name = naptr->replacement;
    }
  }
  else
  {
    DnsResult result = _dns_client->dns_query("_diameter._tcp." + srv_name, ns_t_srv);
    if (!result.records().empty())
    {
      srv_name = result.domain();
    }
  }

  if (!srv_name.empty())
  {
    srv_resolve(srv_name, _af, IPPROTO_TCP, max_peers, targets, 0);
  }
  else
  {
    a_resolve(name, _af, 5060, IPPROTO_TCP, max_peers, targets, 0);
  }
}
