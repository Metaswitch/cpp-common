/**
 * @file fakecounter.h class definition for a fake statistics counter
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef FAKECOUNTER_H__
#define FAKECOUNTER_H__

#include <atomic>
#include <time.h>

#include "counter.h"

/// @class FakeCounter
///
/// Counts events over a set period, pushing the total number as the statistic
class FakeCounter : public Counter
{
public:
  inline FakeCounter(uint_fast64_t period_us = DEFAULT_PERIOD_US) : Counter(period_us) {}

  /// Callback whenever the accumulated statistics are refreshed.   Does nothing.
  virtual void refreshed() {};
};

#endif

