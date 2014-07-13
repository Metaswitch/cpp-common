/**
 * @file fakehttpresolver.hpp Header file for fake HTTP resolver (for testing).
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

#pragma once

#include <string>
#include "log.h"
#include "sas.h"
#include "httpresolver.h"

/// HttpResolver that always returns hard-coded IP address.
class FakeHttpResolver : public HttpResolver
{
public:
  FakeHttpResolver() :
    HttpResolver(NULL, AF_INET), _targets() {}

  FakeHttpResolver(const std::string& ip) :
    HttpResolver(NULL, AF_INET), _targets()
  {
    AddrInfo ai;
    ai.transport = IPPROTO_TCP;
    parse_ip_target(ip, ai.address);
    _targets.push_back(ai);
  }

  FakeHttpResolver(const std::string& ip1, const std::string& ip2) :
    HttpResolver(NULL, AF_INET), _targets()
  {
    AddrInfo ai;
    ai.transport = IPPROTO_TCP;
    parse_ip_target(ip1, ai.address);
    _targets.push_back(ai);
    parse_ip_target(ip2, ai.address);
    _targets.push_back(ai);
  }

  ~FakeHttpResolver() {}

  void resolve(const std::string& host,
               int port,
               int max_targets,
               std::vector<AddrInfo>& targets,
               SAS::TrailId trail)
  {
    targets = _targets;
    // Fix up ports.
    for (std::vector<AddrInfo>::iterator i = targets.begin(); i != targets.end(); ++i)
    {
      i->port = (port != 0) ? port : 80;
    }
  }

  std::vector<AddrInfo> _targets;
};
