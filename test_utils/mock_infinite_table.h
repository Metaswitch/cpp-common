/**
 * @file mock_infinite_table.h
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_INFINITE_TABLE_H_
#define MOCK_INFINITE_TABLE_H_

#include "gmock/gmock.h"
#include "snmp_infinite_timer_count_table.h"

class MockInfiniteTable : public SNMP::InfiniteTimerCountTable
{
public:
  MockInfiniteTable ();

  ~MockInfiniteTable();

  MOCK_METHOD2(increment, void(std::string value, uint32_t count));
  MOCK_METHOD2(decrement, void(std::string value, uint32_t count));
};

#endif
