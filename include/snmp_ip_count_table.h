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

#include "snmp_table.h"
#include "snmp_includes.h"
#include "logger.h"

#ifndef SNMP_IP_COUNT_TABLE_H
#define SNMP_IP_COUNT_TABLE_H

namespace SNMP
{


// Row of counters indexed by RFC 2851 IP addresses
class IPCountRow : public Row
{
public:
  IPCountRow(std::string addrStr) :
    Row(),
    _count(0)
  {
    // Convert the string into a type (IPv4 or IPv6) and a sequence of bytes
    if (inet_pton(AF_INET, addrStr.c_str(), &_addr) == 1) {
      _addr_type = 1; // IPv4
      _addr_len = sizeof(struct in_addr);
    } else if (inet_pton(AF_INET6, addrStr.c_str(), &_addr) == 1) {
      _addr_type = 2; // IPv6
      _addr_len = sizeof(struct in6_addr);
    } else {
      _addr_type = 0; // unknown
      _addr_len = 0;
    }

    // Set the IPAddrType and IPAddr as indexes
    netsnmp_tdata_row_add_index(_row,
                                ASN_INTEGER,
                                &_addr_type,
                                sizeof(int));
 
    netsnmp_tdata_row_add_index(_row,
                                ASN_OCTET_STR,
                                (unsigned char*)&_addr,
                                _addr_len);
    
  };

  ColumnData get_columns()
  {
    // Construct and return a ColumnData with the appropriate values
    ColumnData ret;
    ret[1] = Value::integer(_addr_type);
    ret[2] = Value(ASN_OCTET_STR, (unsigned char*)&_addr, _addr_len);
    ret[3] = Value::uint(_count);
    return ret;
  }

  uint32_t increment() { return ++_count; };
  uint32_t decrement() { return --_count; };

protected:
  int _addr_type;
  union {
    struct in_addr  v4;
    struct in6_addr v6;
  } _addr;
  int _addr_len;
  uint32_t _count;
};

class IPCountTable: public ManagedTable<IPCountRow, std::string>
{
public:
  IPCountTable(std::string name,
                   oid* tbl_oid,
                   int oidlen) :
    ManagedTable<IPCountRow, std::string>(name, tbl_oid, oidlen)
  {
    _tbl.add_index(ASN_INTEGER);
    _tbl.add_index(ASN_OCTET_STR);
    _tbl.set_visible_columns(3, 3);
  }

  IPCountRow* new_row(std::string ip)
  {
    return new IPCountRow(ip);
  }
};

}

#endif
