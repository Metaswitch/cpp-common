/**
 * @file counter.cpp class implementation for a statistics counter
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <vector>
#include "counter.h"

/// Increase the current count by 1.
void Counter::increment(void)
{
  // Update the basic counters and samples.
  _current._count++;
  // Refresh the statistics, if required.
  refresh();
}

/// Refresh our calculations - called at the end of each period, or
/// optionally at other times to get an up-to-date result.
void Counter::refresh(bool force)
{
  // Get the timestamp from the start of the current period, and the timestamp
  // now.
  uint_fast64_t timestamp_us = _current._timestamp_us.load();
  uint_fast64_t timestamp_us_now = get_timestamp_us();

  // If we're forced, or this period is already long enough, read the new
  // values and make the refreshed() callback.
  if ((force ||
      (timestamp_us_now >= timestamp_us + _target_period_us)) &&
      (_current._timestamp_us.compare_exchange_weak(timestamp_us, timestamp_us_now)))
  {
    read(timestamp_us_now - timestamp_us);
    refreshed();
  }
}

/// Reset the counter.
void Counter::reset()
{
  // Get the timestamp now.
  _current._timestamp_us.store(get_timestamp_us());
  // Reset everything else to 0.
  _current._count.store(0);
  _last._count = 0;
}

/// Read the counter and report
/// it as the last set of statistics.
void Counter::read(uint_fast64_t period_us)
{
  // Read the basic statistics, and replace them with 0.
  uint_fast64_t count = _current._count.exchange(0);
  _last._count = count ;
}

/// Callback whenever the accumulated statistics are refreshed. Passes
/// values to zeroMQ.
void StatisticCounter::refreshed()
{
  // Simply construct a vector of count only and pass it to zeroMQ.
  std::vector<std::string> values;
  values.push_back(std::to_string(get_count()));
  _statistic.report_change(values);
}
