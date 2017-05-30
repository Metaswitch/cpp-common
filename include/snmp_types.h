/**
 * @file snmp_types.h
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef SNMP_TYPES_H_
#define SNMP_TYPES_H_

namespace SNMP
{

enum TimePeriodIndexes
{
  scopePrevious5SecondPeriod = 1,
  scopeCurrent5MinutePeriod = 2,
  scopePrevious5MinutePeriod = 3,
};

enum AddrTypes
{
  Unknown = 0,
  IPv4 = 1,
  IPv6 = 2
};

}

#endif

