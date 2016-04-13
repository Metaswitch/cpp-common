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

#include "snmp_ip_count_table.h"
#include "snmp_internal/snmp_table.h"
#include "snmp_internal/snmp_includes.h"
#include "logger.h"

namespace SNMP
{

IPCountRow::IPCountRow(struct in_addr addr) : IPRow(addr), _count(0) {};

IPCountRow::IPCountRow(struct in6_addr addr) : IPRow(addr), _count(0) {};

ColumnData IPCountRow::get_columns()
{
  // Construct and return a ColumnData with the appropriate values
  ColumnData ret;
  ret[1] = Value::integer(_addr_type);
  ret[2] = Value(ASN_OCTET_STR, (unsigned char*)&_addr, _addr_len);
  ret[3] = Value::uint(_count);
  return ret;
}

class IPCountTableImpl: public ManagedTable<IPCountRow, std::string>, public IPCountTable
{
public:
  IPCountTableImpl(std::string name,
                   std::string tbl_oid):
    ManagedTable<IPCountRow, std::string>(name,
                                          tbl_oid,
                                          3,
                                          3, // Only column 3 should be visible
                                          { ASN_INTEGER, ASN_OCTET_STR }) // Types of the index columns
  {}

  IPCountRow* new_row(std::string ip)
  {
    struct in_addr  v4;
    struct in6_addr v6;

    // Convert the string into a type (IPv4 or IPv6) and a sequence of bytes
    if (inet_pton(AF_INET, ip.c_str(), &v4) == 1)
    {
      return new IPCountRow(v4);
    }
    else if (inet_pton(AF_INET6, ip.c_str(), &v6) == 1)
    {
      return new IPCountRow(v6);
    }
    else
    {
      TRC_ERROR("Could not parse %s as an IPv4 or IPv6 address", ip.c_str());
      return NULL;
    }
  }

  IPCountRow* get(std::string key) { return ManagedTable<IPCountRow, std::string>::get(key); };
  void add(std::string key) { ManagedTable<IPCountRow, std::string>::add(key); };
  void remove(std::string key) { ManagedTable<IPCountRow, std::string>::remove(key); };
};

IPCountTable* IPCountTable::create(std::string name, std::string oid)
{
  return new IPCountTableImpl(name, oid);
}

}
