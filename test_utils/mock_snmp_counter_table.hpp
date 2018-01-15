/**
 * @file mock_snmp_counter_table.hpp Mock SNMP counter table class for UT
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_SNMP_COUNTER_TABLE_HPP__
#define MOCK_SNMP_COUNTER_TABLE_HPP__

#include "gmock/gmock.h"
#include "snmp_counter_table.h"

class MockSnmpCounterTable : public SNMP::CounterTable
{
public:
  MOCK_METHOD0(increment, void());
};

#endif
