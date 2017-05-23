/**
 * @file timer_counter.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef TIMER_COUNTER_H
#define TIMER_COUNTER_H

#include "current_and_previous.h"
#include "snmp_statistics_structures.h"

#include <vector>
#include <map>
#include <string>
#include <atomic>

#include "log.h"

class TimerCounter
{
public:
  TimerCounter();
  ~TimerCounter();

  void increment(uint32_t count = 1);
  void decrement(uint32_t count = 1);
  void get_statistics(int index, timespec now, SNMP::SimpleStatistics* stats);

  CurrentAndPrevious<SNMP::ContinuousStatistics> five_second;
  CurrentAndPrevious<SNMP::ContinuousStatistics> five_minute;

private:
  void refresh_statistics(SNMP::ContinuousStatistics* current_data,
                          timespec now,
                          uint32_t interval_ms);
  void write_statistics(SNMP::ContinuousStatistics* current_data, int value_delta);
  void read_statistics(SNMP::ContinuousStatistics* current_data,
                       SNMP::SimpleStatistics* return_data,
                       timespec now,
                       uint32_t interval_ms);
};

#endif
