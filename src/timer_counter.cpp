/**
 * @file timer_counter.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "current_and_previous.h"
#include "timer_counter.h"
#include "limits.h"

TimerCounter::TimerCounter():
  five_second(5000),
  five_minute(300000)
{
  timespec now;
  clock_gettime(CLOCK_REALTIME_COARSE, &now);

  write_statistics(five_second.get_current(now), 0);
  write_statistics(five_second.get_previous(now), 0);
  write_statistics(five_minute.get_current(now), 0);
  write_statistics(five_minute.get_previous(now), 0);
}


TimerCounter::~TimerCounter() {}

void TimerCounter::increment(uint32_t count)
{
  timespec now;
  clock_gettime(CLOCK_REALTIME_COARSE, &now);

  refresh_statistics(five_second.get_current(now), now, five_second.get_interval_ms());
  write_statistics(five_second.get_current(now), count);

  refresh_statistics(five_minute.get_current(now), now, five_minute.get_interval_ms());
  write_statistics(five_minute.get_current(now), count);
}

void TimerCounter::decrement(uint32_t count)
{
  timespec now;
  clock_gettime(CLOCK_REALTIME_COARSE, &now);

  refresh_statistics(five_second.get_current(now), now, five_second.get_interval_ms());
  write_statistics(five_second.get_current(now), -count);

  refresh_statistics(five_minute.get_current(now), now, five_minute.get_interval_ms());
  write_statistics(five_minute.get_current(now), -count);
}

void TimerCounter::get_statistics(int index, timespec now, SNMP::SimpleStatistics* stats)
{
  SNMP::ContinuousStatistics* data = NULL;
  uint32_t interval_ms = 0;
  switch (index)
  {
    case 1:
      data = five_second.get_previous(now);
      interval_ms = five_second.get_interval_ms();
      break;
    case 2:
      data = five_minute.get_current(now);
      interval_ms = five_minute.get_interval_ms();
      break;
    case 3:
      data = five_minute.get_previous(now);
      interval_ms = five_minute.get_interval_ms();
      break;
  }

  refresh_statistics(data, now, interval_ms);
  read_statistics(data, stats, now, interval_ms);
}


void TimerCounter::refresh_statistics(SNMP::ContinuousStatistics* data, timespec now, uint32_t interval_ms)
{
  // Compute the updated sum and sqsum based on the previous values, dependent on
  // how long since an update happened. Additionally update the sum of squares as a
  // rolling total, and update the time of the last update. Also maintain a
  // current value held, that can be used if the period ends.

  uint64_t time_period_start_ms = data->time_period_start_ms.load();
  uint64_t time_period_end_ms = ((time_period_start_ms + interval_ms) / interval_ms) * interval_ms;
  uint64_t time_now_ms = (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
  uint64_t time_comes_first_ms = std::min(time_period_end_ms, time_now_ms);
  uint64_t time_since_last_update = time_comes_first_ms - data->time_last_update_ms.load();

  uint64_t current_value = data->current_value.load();

  data->sum += current_value * time_since_last_update;
  data->sqsum += current_value * current_value * time_since_last_update;
  data->time_last_update_ms.store(time_comes_first_ms);
}

void TimerCounter::write_statistics(SNMP::ContinuousStatistics* data, int value_delta)
{
  // Initialise a new value to be used, and pull
  // the current value from the underlying data.
  uint64_t current_value = data->current_value.load();
  uint64_t new_value;

  // Attempt to calculate the new value, and set it to the atomic beneath.
  // If the atomic value has changed in the mean time, repeat the calculations
  // using the new current value.
  do
  {
    // Ensure value_delta is less than or equal to the current value if negative.
    // If value_delta would cause new_value to underflow, leave new_value as 0.
    if ((value_delta > 0) || ((uint64_t)-value_delta <= current_value))
    {
      new_value = current_value + value_delta;
    }
    else
    {
      new_value = 0;
    }
  } while (!data->current_value.compare_exchange_weak(current_value, new_value));

  // Update the low- and high-water marks.  In each case, we get the current
  // value, decide whether a change is required and then atomically swap it
  // if so, repeating if it was changed in the meantime.  Note that
  // compare_exchange_weak loads the current value into the expected value
  // parameter (lwm or hwm below) if the compare fails.

  uint_fast64_t lwm = data->lwm.load();
  while ((new_value < lwm) &&
         (!data->lwm.compare_exchange_weak(lwm, new_value)))
  {
    // Do nothing.
  }

  uint_fast64_t hwm = data->hwm.load();
  while ((new_value > hwm) &&
         (!data->hwm.compare_exchange_weak(hwm, new_value)))
  {
    // Do nothing.
  }
}

void TimerCounter::read_statistics(SNMP::ContinuousStatistics* data,
                                   SNMP::SimpleStatistics* new_data,
                                   timespec now,
                                   uint32_t interval_ms)
{
  uint64_t hwm = data->hwm.load();
  uint64_t lwm = data->lwm.load();
  uint64_t sum = data->sum.load();
  uint64_t sqsum = data->sqsum.load();
  uint64_t current_value = data->current_value.load();

  uint64_t time_period_start_ms = data->time_period_start_ms.load();
  uint64_t time_period_end_ms = ((time_period_start_ms + interval_ms) / interval_ms) * interval_ms;
  uint64_t time_now_ms = (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
  uint64_t time_comes_first_ms = std::min(time_period_end_ms, time_now_ms);
  uint64_t period_count = (time_comes_first_ms - time_period_start_ms);

  // Save off the data in the stats parameter
  new_data->average = current_value;

  if (period_count > 0)
  {
    uint64_t average = sum / period_count;
    new_data->average = average;
    new_data->variance = (sqsum / period_count) - (average * average);
  }
  new_data->current_value = current_value;
  new_data->hwm = hwm;
  new_data->lwm = (lwm == ULONG_MAX) ? 0 : lwm;
}
