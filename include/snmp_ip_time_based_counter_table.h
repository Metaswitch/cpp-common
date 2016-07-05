/**
 * @file snmp_ip_timed_based_count_table.h
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2016 Metaswitch Networks Ltd
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

#ifndef SNMP_IP_TIMED_BASED_COUNT_TABLE_H_
#define SNMP_IP_TIMED_BASED_COUNT_TABLE_H_

namespace SNMP
{

class IPTimeBasedCounterTable
{
public:
  virtual ~IPTimeBasedCounterTable() {};

  /// Create a new instance of the table.
  ///
  /// @param name - The name of the table.
  /// @param oid  - The OID subtree that the table lives within.
  ///
  /// @return     - The table instance.
  static IPTimeBasedCounterTable* create(std::string name, std::string oid);

  /// Add rows to the table for the specified IP address. If this IP already
  /// exists in the table, an additional reference count will be added for
  /// it.
  ///
  /// Calls to add_ip and remove_ip should be balanced.
  ///
  /// @param ip - The IP address to add. Must be a valid IPv4 or IPv6 IP
  ///             address.
  virtual void add_ip(const std::string& ip) = 0;

  /// Removes rows for the specified IP address from the table. If this IP has
  /// been added multiple times, this just removes one from the reference count.
  ///
  /// Calls to add_ip and remove_ip should be balanced.
  ///
  /// @param ip - The IP address to remove. Must be a valid IPv4 or IPv6 IP
  ///             address.
  virtual void remove_ip(const std::string& ip) = 0;

  /// Increment the count for the given IP. The IP address must have been
  /// previously added to the table by calling `add_ip`. If it has not, the
  /// increment is ignored.
  ///
  /// @param ip - The IP address to increment the stat for.
  virtual void increment(const std::string& ip) = 0;

protected:
  IPTimeBasedCounterTable() {};
};

}

#endif

