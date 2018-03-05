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

#ifndef EVENT_STATISTIC_ACCUMULATOR_H
#define EVENT_STATISTIC_ACCUMULATOR_H

// This file defines the event statistics class which is used for accumulating
// single measurements of an event (e.g. the latency of SIP requests) and
// calculating a set of statistics based on those measurements: HWM, LWM,
// Count, Average (Mean) and Variance.

namespace SNMP
{

// Structure used to hold calculated statistics.
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

  // Accumulate data about an additional event.  E.g. for SIP request
  // latencies, this would be called each time a response is received to track
  // the latency of that request.
  void accumulate(uint32_t sample);

  // Compute the current statistics values and fill them in in the supplied
  // EventStatistics structure.
  void get_stats(EventStatistics &stats);

  // Reset all of the statistics.
  void reset(uint64_t periodstart, EventStatisticAccumulator* previous = NULL);

private:
  // The quantities that we track dynamically as we receive information about
  // individual events.  These are sufficient to calculate all of the
  // statistics that we need to be able to report.
  std::atomic_uint_fast64_t _count;
  std::atomic_uint_fast64_t _sum;
  std::atomic_uint_fast64_t _sqsum;
  std::atomic_uint_fast64_t _hwm;
  std::atomic_uint_fast64_t _lwm;
};

}

#endif
