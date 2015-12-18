/**
 * @file astaire_resolver.h
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

