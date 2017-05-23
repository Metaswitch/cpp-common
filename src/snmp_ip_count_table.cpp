/**
 * @file snmp_ip_count_table.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
