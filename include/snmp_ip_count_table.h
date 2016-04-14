/**
 * @file snmp_ip_count_table.h
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2015 Metaswitch Networks Ltd
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
#include <netinet/in.h>
#include <arpa/inet.h>

#include "logger.h"
#include "snmp_row.h"
#include "snmp_ip_row.h"

#ifndef SNMP_IP_COUNT_TABLE_H
#define SNMP_IP_COUNT_TABLE_H

// This file contains the interface for tables which:
//   - are indexed by IP address and IP address type
//   - report a count for each IP address
//
// It also contains the interface for their rows.
//
// To use an IP count table, simply create one, call `get` on it to create appropriate rows, and
// call `increment` or `decrement` on those rows as necessary:
//
// SNMP::IPCountTable* xdm_cxns_table = SNMP::IPCountTable::create("connections_to_homer", ".1.2.3");
// xdm_cxns_table->get("10.0.0.1")->increment();
// xdm_cxns_table->get("10.0.0.2")->decrement();
//
// IPCountRow objects are automatically created when needed, but need to be explicitly deleted (with
// `remove`):
//
// xdm_cxns_table->remove("10.0.0.1");


namespace SNMP
{

// Row of counters indexed by RFC 2851 IP addresses
class IPCountRow : public IPRow
{
public:
  IPCountRow(struct in_addr addr);
  IPCountRow(struct in6_addr addr);

  uint32_t increment() { return ++_count; };
  uint32_t decrement() { return --_count; };

  ColumnData get_columns();

protected:
  uint32_t _count;
};

class IPCountTable
{
public:
  static IPCountTable* create(std::string name, std::string oid);
  virtual ~IPCountTable() {};
  virtual IPCountRow* get(std::string key) = 0;
  virtual void add(std::string key) = 0;
  virtual void remove(std::string key) = 0;
protected:
  IPCountTable() {};
};

}

#endif
