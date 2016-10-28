/**
 * @file snmp_counter_by_scope_table.h
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
 * software and licensed und er the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include <vector>
#include <map>
#include <string>

#ifndef SNMP_COUNTER_BY_SCOPE_TABLE_H
#define SNMP_COUNTER_BY_SCOPE_TABLE_H

// This file contains the interface for tables which:
//   - are indexed by time period and scope (node type)
//   - increment a single counter over time
//   - report a single column for each time period with that count
//
// This is defined as an interface in order not to pollute the codebase with netsnmp include files
// (which indiscriminately #define things like READ and WRITE).

namespace SNMP
{
class CounterByScopeTable
{
public:
  static CounterByScopeTable* create(std::string name, std::string oid);
  virtual void increment() = 0;
  virtual ~CounterByScopeTable() {};

protected:
  CounterByScopeTable() {};
};
}
#endif
