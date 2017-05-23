/**
 * @file current_and_previous.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef CURRENT_AND_PREVIOUS_H
#define CURRENT_AND_PREVIOUS_H

#include <vector>
#include <map>
#include <string>
#include <atomic>
#include "logger.h"

template <class T> class CurrentAndPrevious
{
// For the data type T, store a current and previous period (duration defined by _interval) of
// the data. (For example, the current five seconds of data and the previous five seconds of
// data).

public:

  CurrentAndPrevious(int interval_ms):
    current(&a),
    previous(&b),
    _interval_ms(interval_ms),
    a(),
    b()
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);
    uint64_t time_now_ms = (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
    _tick = (now.tv_sec / (_interval_ms / 1000));
    a.reset(time_now_ms, NULL);
    b.reset(time_now_ms - _interval_ms, NULL);
  }

  // Rolls the current period over into the previous period if necessary.
  void update_time(struct timespec now)
  {
    // Count of how many _interval periods have passed since the epoch
    uint64_t new_tick = (now.tv_sec / (_interval_ms / 1000));

    // Count of how many _interval periods have passed since the last change
    uint32_t tick_difference = new_tick - _tick;
    _tick = new_tick;

    if (tick_difference == 1)
    {
      T* tmp;
      tmp = previous.load();
      previous.store(current);
      tmp->reset(new_tick * _interval_ms, current.load());
      current.store(tmp);
    }
    else if (tick_difference > 1)
    {
      current.load()->reset(new_tick * _interval_ms, current.load());
      previous.load()->reset((new_tick - 1) * _interval_ms, current.load());
    }
  }

  T* get_current() {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);
    return get_current(now);
  }

  T* get_previous() {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);
    return get_previous(now);
  }

  T* get_current(struct timespec now) { update_time(now); return current.load(); }
  T* get_previous(struct timespec now) { update_time(now); return previous.load(); }
  uint32_t get_interval_ms() { return _interval_ms; }

protected:
    std::atomic<T*> current;
    std::atomic<T*> previous;
    uint32_t _interval_ms;
    uint32_t _tick;
    T a;
    T b;

};

#endif
