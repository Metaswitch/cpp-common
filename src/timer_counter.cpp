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

  struct timespec now;
  clock_gettime(CLOCK_REALTIME_COARSE, &now);

  update_values(five_second, current_value, now);
  update_values(five_minute, current_value, now);
}

void TimerCounter::decrement()
{
  current_value++;

  struct timespec now;
  clock_gettime(CLOCK_REALTIME_COARSE, &now);

  update_values(five_second, current_value, now);
  update_values(five_minute, current_value, now);
}

void TimerCounter::update_values(timespec now)
{
  update_values(five_second, current_value, now);
  update_values(five_minute, current_value, now);
}

SNMP::ContinuousStatistics* TimerCounter::get_values(int index, timespec now)
{
  switch (index)
  {
    case 1: return five_second.get_previous(now);
    case 2: return five_minute.get_current(now);
    case 3: return five_minute.get_previous(now);
  }
  return NULL;
}

uint32_t TimerCounter::get_interval_ms(int index)
{
  switch (index)
  {
    case 1: return five_second.get_interval_ms();
    case 2: return five_minute.get_interval_ms();
    case 3: return five_minute.get_interval_ms();
  }
  return 0;
}

void TimerCounter::update_values(CurrentAndPrevious<SNMP::ContinuousStatistics>& data, uint32_t sample, timespec now)
{
  SNMP::ContinuousStatistics* current_data = data.get_current(now);

  TRC_DEBUG("Accumulating sample %uui into continuous accumulator statistic", sample);

  // Compute the updated sum and sqsum based on the previous values, dependent on
  // how long since an update happened. Additionally update the sum of squares as a
  // rolling total, and update the time of the last update. Also maintain a
  // current value held, that can be used if the period ends.

  uint64_t time_since_last_update = ((now.tv_sec * 1000) + (now.tv_nsec / 1000000))
    - (current_data->time_last_update_ms.load());
  uint32_t current_value = current_data->current_value.load();

  current_data->time_last_update_ms = (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
  current_data->sum += current_value * time_since_last_update;
  current_data->sqsum += current_value * current_value * time_since_last_update;
  current_data->current_value = sample;

  // Update the low- and high-water marks.  In each case, we get the current
  // value, decide whether a change is required and then atomically swap it
  // if so, repeating if it was changed in the meantime.  Note that
  // compare_exchange_weak loads the current value into the expected value
  // parameter (lwm or hwm below) if the compare fails.
  uint_fast64_t lwm = current_data->lwm.load();
  while ((sample < lwm) &&
         (!current_data->lwm.compare_exchange_weak(lwm, sample)))
  {
    // Do nothing.
  }
  uint_fast64_t hwm = current_data->hwm.load();
  while ((sample > hwm) &&
         (!current_data->hwm.compare_exchange_weak(hwm, sample)))
  {
    // Do nothing.
  }
}
