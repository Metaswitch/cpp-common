/**
 * @file mock_snmp_ip_time_based_counter_table.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_SNMP_IP_TIME_BASED_COUNTER_TABLE_H__
#define MOCK_SNMP_IP_TIME_BASED_COUNTER_TABLE_H__

#include "snmp_ip_time_based_counter_table.h"

class MockIPTimeBasedCounterTable : public SNMP::IPTimeBasedCounterTable
{
public:
  MOCK_METHOD1(add_ip, void(const std::string&));
  MOCK_METHOD1(remove_ip, void(const std::string&));
  MOCK_METHOD1(increment, void(const std::string&));
};

#endif

