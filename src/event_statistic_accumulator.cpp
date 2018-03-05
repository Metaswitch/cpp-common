/**
 * @file event_statistic_accumulator.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
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

#include "log.h"
#include "event_statistic_accumulator.h"
#include "limits.h"

namespace SNMP
{

EventStatisticAccumulator::EventStatisticAccumulator()
{
  // Start by resetting all of the statistics.
  reset(0);
}

void EventStatisticAccumulator::accumulate(uint32_t sample)
{
  TRC_DEBUG("Accumulate %u for %p", sample, this);
  _count++;

  // Just keep a running total as we go along, so we can calculate the average
  // and variance on request.
  _sum += sample;
  _sqsum += (sample * sample);

  // Update the low- and high-water marks.  In each case, we get the current
  // value, decide whether a change is required and then atomically swap it
  // if so, repeating if it was changed in the meantime (on a different thread).
  // Note that compare_exchange_weak loads the current value into the expected
  // value parameter (lwm or hwm below) if the compare fails.
  uint_fast64_t lwm = _lwm.load();
  while ((sample < lwm) &&
         (!_lwm.compare_exchange_weak(lwm, sample)))
  {
    // Do nothing.
  }
  uint_fast64_t hwm = _hwm.load();
  while ((sample > hwm) &&
         (!_hwm.compare_exchange_weak(hwm, sample)))
  {
    // Do nothing.
  }
}

void EventStatisticAccumulator::get_stats(EventStatistics &stats)
{
  // Compute the stats and return them in the supplied EventStatistics
  // structure.
  stats.count = _count;

  if (_count > 0)
  {
    // Get local copies of the values that we are working with atomically.
    // Note that e.g. _sum is a std::atomic and so assignment is guaranteed
    // atomic.
    uint_fast64_t sum = _sum;
    uint_fast64_t sqsum = _sqsum;

    // Calculate the average and the variance from the stored sum and sum-of-squares.
    stats.mean = sum/stats.count;
    stats.variance = ((sqsum * stats.count) - (sum * sum)) / (stats.count * stats.count);
    stats.hwm = _hwm;
    stats.lwm = _lwm;
  }
  else
  {
    stats.mean = 0;
    stats.variance = 0;
    stats.lwm = 0;
    stats.hwm = 0;
  }
}

void EventStatisticAccumulator::reset(uint64_t periodstart, EventStatisticAccumulator* previous)
{
  _count = 0;
  _sum = 0;
  _sqsum = 0;
  _lwm = ULONG_MAX;
  _hwm = 0;
}

}
