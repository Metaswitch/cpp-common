/**
 * @file snmp_ip_row.cpp
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "snmp_internal/snmp_table.h"
#include "snmp_internal/snmp_includes.h"

#include "snmp_ip_row.h"

namespace SNMP
{

IPRow::IPRow(struct in_addr addr) :
  Row(),
  _addr_type(AddrTypes::IPv4),
  _addr_len(sizeof(struct in_addr))
{
  _addr.v4 = addr;
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

IPRow::IPRow(struct in6_addr addr) :
  Row(),
  _addr_type(AddrTypes::IPv6),
  _addr_len(sizeof(struct in6_addr))
{
  _addr.v6 = addr;
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

}
