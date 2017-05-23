/**
 * @file snmp_ip_row.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef SNMP_IP_ROW_H_
#define SNMP_IP_ROW_H_

#include <netinet/in.h>
#include <arpa/inet.h>

#include "snmp_row.h"
#include "snmp_types.h"

namespace SNMP
{

// Row indexed by RFC 2851 IP addresses
class IPRow : public Row
{
public:
  IPRow(struct in_addr addr);
  IPRow(struct in6_addr addr);

protected:
  int _addr_type;
  int _addr_len;
  union
  {
    struct in_addr  v4;
    struct in6_addr v6;
  } _addr;
};

}

#endif

