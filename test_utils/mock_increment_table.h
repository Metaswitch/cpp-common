/**
 * @file mock_increment_table.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_INCREMENT_TABLE_H_
#define MOCK_INCREMENT_TABLE_H_

#include "gmock/gmock.h"
#include "snmp_continuous_increment_table.h"

class MockIncrementTable : public SNMP::ContinuousIncrementTable
{
public:
  MockIncrementTable ();

  ~MockIncrementTable();

  MOCK_METHOD1(increment, void(uint32_t value));
  MOCK_METHOD1(decrement, void(uint32_t value));
};

#endif
