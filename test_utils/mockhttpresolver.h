/**
 * @file mockhttpresolver.h Mock HttpResolver
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

#ifndef MOCKHTTPRESOLVER_H__
#define MOCKHTTPRESOLVER_H__

#include "gmock/gmock.h"
#include "httpresolver.h"
#include "fake_iterator.h"

using ::testing::SetArgReferee;

class MockHttpResolver : public HttpResolver
{
public:
  MockHttpResolver() : HttpResolver(nullptr,0,0,0) {}
  ~MockHttpResolver() {}

  FakeIterator resolve_iter(const std::string& host,
                            int port,
                            SAS::TrailId trail)
  {

    for (std::vector<AddrInfo>::iterator it = targets.begin();
         it != targets.end();
         ++it)
    {
      it->port = (port != 0) ? port : 80;
    }

    return FakeIterator(targets);
  }

  MOCK_METHOD1(blacklist, void(const AddrInfo& ai));
  MOCK_METHOD1(success, void(const AddrInfo& ai));
  MOCK_METHOD1(untested, void(const AddrInfo& ai));

  std::vector<AddrInfo> targets;

  /// Creates a single AddrInfo target from the given IP address string, and
  /// optional port.
  static AddrInfo create_target(std::string address_str, int port = 80)
  {
    AddrInfo ai;
    BaseResolver::parse_ip_target(address_str, ai.address);
    ai.port = port;
    ai.transport = IPPROTO_TCP;
    return ai;
  }

  /// Creates a vector of count AddrInfo targets, beginning from 3.0.0.0 and
  /// incrementing by one each time.
  static std::vector<AddrInfo> create_targets(int count)
  {
    std::vector<AddrInfo> targets;
    std::stringstream os;
    for (int i = 0; i < count; ++i)
    {
      os << "3.0.0." << i;
      targets.push_back(create_target(os.str()));
      os.str(std::string());
    }
    return targets;
  }
};

#endif
