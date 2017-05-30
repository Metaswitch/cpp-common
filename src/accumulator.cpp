/**
 * @file accumulator.cpp class implementation for a statistics accumulator
 *
 * Copyright (C) Metaswitch Networks 2014
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <vector>

#include "accumulator.h"

/// Accumulate a sample into our results.
void Accumulator::accumulate(unsigned long sample)
{
  // Update the basic counters and samples.
  _current._n++;
  _current._sigma += sample;
  _current._sigma_squared += sample * sample;

  // Update the low- and high-water marks.  In each case, we get the current
  // value, decide whether a change is required and then atomically swap it
  // if so, repeating if it was changed in the meantime.  Note that
  // compare_exchange_weak loads the current value into the expected value
  // parameter (lwm or hwm below) if the compare fails.
  uint_fast64_t lwm = _current._lwm.load();
  while ((sample < lwm) &&
	 (!_current._lwm.compare_exchange_weak(lwm, sample)))
  {
    // Do nothing.
  }
  uint_fast64_t hwm = _current._hwm.load();
  while ((sample > hwm) &&
	 (!_current._hwm.compare_exchange_weak(hwm, sample)))
  {
    // Do nothing.
  }

  // Refresh the statistics, if required.
  refresh();
}

/// Refresh our calculations - called at the end of each period, or
/// optionally at other times to get an up-to-date result.
void Accumulator::refresh(bool force)
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

/// Reset the accumulator.
void Accumulator::reset()
{
  // Get the timestamp now.
  _current._timestamp_us.store(get_timestamp_us());
  // Reset everything else to 0.
  _current._n.store(0);
  _current._sigma.store(0);
  _current._sigma_squared.store(0);
  _current._lwm.store(MAX_UINT_FAST64);
  _current._hwm.store(0);
  _last._n = 0;
  _last._mean = 0;
  _last._variance = 0;
  _last._lwm = 0;
  _last._hwm = 0;
}

/// Read the accumulated statistics, calculate their properties and report
/// them as the last set of statistics.
void Accumulator::read(uint_fast64_t period_us)
{
  // Read the basic statistics, and replace them with 0.
  uint_fast64_t n = _current._n.exchange(0);
  uint_fast64_t sigma = _current._sigma.exchange(0);
  uint_fast64_t sigma_squared = _current._sigma_squared.exchange(0);
  // Scale n by the period.
  _last._n = n * period_us / _target_period_us;
  // Calculate the mean in the obvious way (avoiding division by 0.
  uint_fast64_t mean = (n > 0) ? sigma / n : 0;
  _last._mean = mean;
  // Calculate variance as mean of squares minus square of mean.
  _last._variance = (n > 0) ? ((sigma_squared / n) - (mean * mean)) : 0;
  // Read low- and high-water marks, fixing low-water mark to 0 if there were
  // no samples in the period.
  uint_fast64_t lwm = _current._lwm.exchange(MAX_UINT_FAST64);
  _last._lwm = (n > 0) ? lwm : 0;
  _last._hwm = _current._hwm.exchange(0);
}

/// Callback whenever the accumulated statistics are refreshed.  Passes
/// values to zeroMQ.
void StatisticAccumulator::refreshed()
{
  // Simply construct a vector of mean, variance and water marks and pass it
  // to zeroMQ.
  std::vector<std::string> values;
  values.push_back(std::to_string(get_mean()));
  values.push_back(std::to_string(get_variance()));
  values.push_back(std::to_string(get_lwm()));
  values.push_back(std::to_string(get_hwm()));
  values.push_back(std::to_string(get_n()));
  _statistic.report_change(values);
}
