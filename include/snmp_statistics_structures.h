/**
 * @file snmp_statistics_structures.h
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2015 Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
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
