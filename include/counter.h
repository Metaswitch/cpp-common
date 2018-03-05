/**
 * @file counter.h class definition for a statistics counter
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef COUNTER_H__
#define COUNTER_H__

#include <atomic>
#include <time.h>

#include "statrecorder.h"
#include "zmq_lvc.h"

/// @class Counter
///
/// Counts events over a set period, pushing the total number as the statistic
class Counter : public StatRecorder
{
public:

  inline Counter(uint_fast64_t period_us = DEFAULT_PERIOD_US) :
           StatRecorder(period_us)
  {
    reset();
  }

  virtual ~Counter() {}

  /// Increment function
  void increment(void);

  /// Refresh our calculations - called at the end of each period, or
  /// optionally at other times to get an up-to-date result.
  virtual void refresh(bool force = false);

  /// Get number of results in last period.
  inline uint_fast64_t get_count() { return _last._count; }

  virtual void reset();

private:
  /// Current accumulated count.
  struct {
    std::atomic_uint_fast64_t _timestamp_us;
    std::atomic_uint_fast64_t _count;
  } _current;

  /// Count accumulated over the previous period.
  struct {
    volatile uint_fast64_t _count;
  } _last;

  virtual void read(uint_fast64_t period_us);
};

/// @class StatisticCounter
///
/// Counts and reports value as a zeroMQ-based statistic.
class StatisticCounter : public Counter
{
public:
  /// Constructor.
  inline StatisticCounter(std::string statname,
                          LastValueCache* lvc,
                          uint_fast64_t period_us = DEFAULT_PERIOD_US) :
    Counter(period_us),
    _statistic(statname, lvc)
  {}

  /// Callback whenever the accumulated statistics are refreshed. Passes
  /// values to zeroMQ.
  virtual void refreshed();

private:
  /// The zeroMQ-based statistic to report to.
  Statistic _statistic;
};

#endif

