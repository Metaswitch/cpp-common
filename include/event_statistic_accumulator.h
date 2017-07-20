/**
 * @file event_statistic_accumulator.h
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

#include "logger.h"

#ifndef EVENT_STATISTIC_ACCUMULATOR_H
#define EVENT_STATISTIC_ACCUMULATOR_H

// This file defines the event statistics class which is used for accumulating
// single measurements of an event (e.g. the latency of SIP requests) and
// calculating a set of statistics based on those measurements: HWM, LWM,
// Count, Average (Mean) and Variance.

namespace SNMP
{

struct EventStatistics
{
  uint_fast64_t count;
  uint_fast64_t mean;
  uint_fast64_t variance;
  uint_fast64_t lwm;
  uint_fast64_t hwm;
};


class EventStatisticAccumulator
{
public:
  EventStatisticAccumulator();
  virtual ~EventStatisticAccumulator() {};

  // Accumulate a sample into the underlying statistics.
  void accumulate(uint32_t sample);

  // Compute the current statistics values.
  inline void get_stats(EventStatistics &stats);

  // Reset all of the statistics.
  void reset(uint64_t periodstart, EventStatisticAccumulator* previous = NULL);

private:
  std::atomic_uint_fast64_t _count;
  std::atomic_uint_fast64_t _sum;
  std::atomic_uint_fast64_t _sqsum;
  std::atomic_uint_fast64_t _hwm;
  std::atomic_uint_fast64_t _lwm;
};

}

#endif
