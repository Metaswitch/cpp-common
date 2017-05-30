/**
 * @file statrecorder.h abstract class definition for a statistics accumulator
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef STATRECORDER_H__
#define STATRECORDER_H__

#include "statistic.h"

class StatRecorder
{
public:
  /// Default accumulation period, in microseconds.
  static const uint_fast64_t DEFAULT_PERIOD_US = 5 * 1000 * 1000;

  /// Constructor.
  inline StatRecorder(uint_fast64_t period_us = DEFAULT_PERIOD_US) :
           _target_period_us(period_us) {}

  /// Refresh our calculations - called at the end of each period, or
  /// optionally at other times to get an up-to-date result.
  /// must be implemented by subclass
  virtual void refresh(bool force = false) = 0;

  /// Resets the accumulator - must be implemented by subclass
  virtual void reset() = 0;

  /// Callback whenever the accumulated statistics are refreshed. Default is
  /// to do nothing.
  /// must be implemented by subclass
  virtual void refreshed() = 0;

protected:
  /// Maximum value of a uint_fast64_t (assuming 2s-complement). There is a
  /// #define for this, but it's unavailable in C++.
  static const uint_fast64_t MAX_UINT_FAST64 = ~((uint_fast64_t)0);

  /// Target period (in microseconds) over which samples are accumulated.
  /// Might be inaccurate due to timing errors, or because events don't come
  /// in frequently enough.
  uint_fast64_t _target_period_us;

  /// Get a timestamp in microseconds.
  inline uint_fast64_t get_timestamp_us()
  {
    uint_fast64_t timestamp = 0;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
    {
      timestamp = (ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
    }
    return timestamp;
  }

private:
  /// Read the accumulated statistics, calculate their properties and report
  /// them as the last set of statistics. Must be implemented by subclass
  virtual void read(uint_fast64_t period_us) = 0;
};

#endif
