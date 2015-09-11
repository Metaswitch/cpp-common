/**
 * @file timer_counter.cpp
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

#include "snmp_internal/current_and_previous.h"
#include "timer_counter.h"
#include "limits.h"

TimerCounter::TimerCounter():
  five_second(5000),
  five_minute(300000) {}


TimerCounter::~TimerCounter() {}

void TimerCounter::increment()
{
  current_value++;
  report_values();
}

void TimerCounter::decrement()
{
  current_value--;
  report_values();
}

void TimerCounter::report_values() {
  struct timespec now;
  clock_gettime(CLOCK_REALTIME_COARSE, &now);

  SNMP::SimpleStatistics stats;

  update_values(five_second.get_current(now),
                five_second.get_interval_ms(),
                current_value,
                now,
                &stats);

  update_values(five_minute.get_current(now),
                five_minute.get_interval_ms(),
                current_value,
                now,
                &stats);
}


void TimerCounter::get_values(int index, timespec now, SNMP::SimpleStatistics* stats)
{
  switch (index)
  {
    case 1: update_values(five_second.get_previous(now),
                                 five_second.get_interval_ms(),
                                 current_value,
                                 now,
                                 stats);
    case 2: update_values(five_minute.get_current(now),
                                 five_minute.get_interval_ms(),
                                 current_value,
                                 now,
                                 stats);
    case 3: update_values(five_minute.get_previous(now),
                                 five_minute.get_interval_ms(),
                                 current_value,
                                 now,
                                 stats);
  }
}

void TimerCounter::update_values(SNMP::ContinuousStatistics* data,
                                                       uint32_t interval_ms,
                                                       uint32_t sample,
                                                       timespec now,
                                                       SNMP::SimpleStatistics* new_data)
{
  TRC_DEBUG("Accumulating sample of value %u into continuous accumulator statistic", sample);

  // Compute the updated sum and sqsum based on the previous values, dependent on
  // how long since an update happened. Additionally update the sum of squares as a
  // rolling total, and update the time of the last update. Also maintain a
  // current value held, that can be used if the period ends.

  uint64_t time_since_last_update = ((now.tv_sec * 1000) + (now.tv_nsec / 1000000))
                                    - (data->time_last_update_ms.load());

  uint64_t time_period_start_ms = data->time_period_start_ms.load();
  uint64_t time_period_end_ms = ((time_period_start_ms + interval_ms) / interval_ms) * interval_ms;
  uint64_t time_now_ms = (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
  uint64_t time_comes_first_ms = std::min(time_period_end_ms, time_now_ms);
  uint64_t period_count = (time_comes_first_ms - time_period_start_ms);

  uint64_t sum = data->sum.load();
  uint64_t sqsum = data->sqsum.load();

  sum += sample * time_since_last_update;
  sqsum += sample * sample * time_since_last_update;

  // Update the low- and high-water marks.  In each case, we get the current
  // value, decide whether a change is required and then atomically swap it
  // if so, repeating if it was changed in the meantime.  Note that
  // compare_exchange_weak loads the current value into the expected value
  // parameter (lwm or hwm below) if the compare fails.

  TRC_DEBUG("sum: %u", sum);
  TRC_DEBUG("sqsum: %u", sqsum);
  TRC_DEBUG("current_value: %u", sample);

  uint_fast64_t lwm = data->lwm.load();
  while ((sample < lwm) &&
         (!data->lwm.compare_exchange_weak(lwm, sample)))
  {
    // Do nothing.
  }
  uint_fast64_t hwm = data->hwm.load();
  while ((sample > hwm) &&
         (!data->hwm.compare_exchange_weak(hwm, sample)))
  {
    // Do nothing.
  }

  TRC_DEBUG("lwm: %u", lwm);
  TRC_DEBUG("hwm: %u", hwm);

  if (period_count > 0)
  {
    new_data->average = sum / period_count;
    new_data->variance = ((sqsum * period_count) - (sum * sum)) /
                          (period_count * period_count);
  }
  new_data->hwm = hwm;
  new_data->lwm = lwm == ULONG_MAX ? 0 : lwm;
}
