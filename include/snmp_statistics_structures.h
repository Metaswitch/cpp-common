/**
 * @file snmp_statistics_structures.h
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <vector>
#include <map>
#include <string>
#include <atomic>
#include "limits.h"

#ifndef SNMP_STATISTICS_STRUCTURES_H
#define SNMP_STATISTICS_STRUCTURES_H

// This file contains the different underlying structures used in SNMP statistics,
// along with their reset method

namespace SNMP
{

// A simple scalar statistsic
struct Scalar
{
  uint32_t value;
};

// A simple counting statistic
struct SingleCount
{
  uint64_t count;
  void reset(uint64_t time_periodstart, SingleCount* previous = NULL) { count = 0; };
};

// Contains a count for attempts, successes and failures
struct SuccessFailCount
{
  std::atomic_uint_fast64_t attempts;
  std::atomic_uint_fast64_t successes;
  std::atomic_uint_fast64_t failures;

  void reset(uint64_t time_periodstart, SuccessFailCount* previous = NULL)
  {
    attempts = 0;
    successes = 0;
    failures = 0;
  }
};

// Contains values to calculate statistics to persist across periods, and has
// fields that support continuous data (i.e. defined over the entire period)
struct ContinuousStatistics
{
  std::atomic_uint_fast64_t count;
  std::atomic_uint_fast64_t current_value;
  std::atomic_uint_fast64_t time_last_update_ms;
  std::atomic_uint_fast64_t time_period_start_ms;
  std::atomic_uint_fast64_t sum;
  std::atomic_uint_fast64_t sqsum;
  std::atomic_uint_fast64_t hwm;
  std::atomic_uint_fast64_t lwm;

  ContinuousStatistics()
  {
    count = 0;
    current_value = 0;
    time_last_update_ms = 0;
    time_period_start_ms = 0;
    sum = 0;
    sqsum = 0;
    hwm = 0;
    lwm = 0;
  }

  ContinuousStatistics(const ContinuousStatistics &other)
  {
    count.store(other.count.load());
    current_value.store(other.current_value.load());
    time_last_update_ms.store(other.time_last_update_ms.load());
    time_period_start_ms.store(other.time_period_start_ms.load());
    sum.store(other.sum.load());
    sqsum.store(other.sqsum.load());
    hwm.store(other.hwm.load());
    lwm.store(other.lwm.load());
  }

/*  ContinuousStatistics& operator=(const ContinuousStatistics& other)
  {
    count.store(other.count.load());
    current_value.store(other.current_value.load());
    time_last_update_ms.store(other.time_last_update_ms.load());
    time_period_start_ms.store(other.time_period_start_ms.load());
    sum.store(other.sum.load());
    sqsum.store(other.sqsum.load());
    hwm.store(other.hwm.load());
    lwm.store(other.lwm.load());
    return *this;
  }*/

  void reset(uint64_t periodstart_ms, ContinuousStatistics* previous = NULL)
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    // At time 0, all incrementing values should be 0
    count.store(0);
    sum.store(0);
    sqsum.store(0);

    // Carry across the previous values from the last table,
    // allowing us to set current, lwm and hwm.
    if (previous != NULL)
    {
      current_value.store(previous->current_value.load());
      lwm.store(previous->current_value.load());
      hwm.store(previous->current_value.load());
    }
    // Without any new data, default the values to 0
    else
    {
      current_value.store(0);
      lwm.store(ULONG_MAX);
      hwm.store(0);
    }

    // Given a ridiculuous periodstart, default the value
    // to the current time
    if (periodstart_ms == 0)
    {
      uint64_t time_now_ms = (now.tv_sec * 1000) + (now.tv_nsec / 1000000);

      time_last_update_ms.store(time_now_ms);
      time_period_start_ms.store(time_now_ms);
    }
    // Set the last update time to be the start of the period
    // Letting us calculate the incrementing values more accurately in
    // accumulate() or get_columns()
    // (As they were set to 0 above)
    else
    {
      time_last_update_ms.store(periodstart_ms);
      time_period_start_ms.store(periodstart_ms);
    }
  }
};

struct SimpleStatistics
{
  uint64_t average = 0;
  uint64_t variance = 0;
  uint64_t current_value = 0;
  uint64_t hwm = 0;
  uint64_t lwm = ULONG_MAX;
  uint64_t count = 0;
};
}
#endif
