/**
 * @file barrier.h Barrier synchronization primitive.
 *
 * Project Clearwater - IMS in the cloud.
 * Copyright (C) 2015  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#ifndef BARRIER_H_
#define BARRIER_H_

/// An implementation of the barrier synchronozation pattern. The barrier has a
/// capacity of N and threads that reach the barrier block until the Nth thread
/// arrives, at which point they all unblock.
///
/// This implementation allows the barrier to be reused, so that threads N+1 to
/// 2N-1 (inclusive) block until thread 2N arrives.
class Barrier
{
public:
  Barrier(unsigned int capacity) :
    _capacity(capacity), _waiters(0), _trigger_count(0)
  {
    pthread_mutex_init(&_mutex, NULL);
    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
    pthread_cond_init(&_cond, &cond_attr);
    pthread_condattr_destroy(&cond_attr);
  }

  ~Barrier()
  {
    pthread_cond_destroy(&_cond);
    pthread_mutex_destroy(&_mutex);
  }

  /// Called when a thread arrives at the barrier.
  ///
  /// @param timeout_us the maximum time (in microseconds) to wait for the
  ///   barrier to trigger.
  /// @return true if the barrier triggered successfully, false if the arrive
  ///   call timed out.
  bool arrive(uint64_t timeout_us = 0)
  {
    bool success = true;
    timespec timeout_ts;

    if (timeout_us != 0)
    {
      // Calculate the time at which the arrive call should timeout.
      clock_gettime(CLOCK_MONOTONIC, &timeout_ts);
      timeout_ts.tv_nsec += (timeout_us % 1000000) * 1000;
      timeout_ts.tv_sec += (timeout_us / 1000000);

      if (timeout_ts.tv_nsec >= 1000000000)
      {
        timeout_ts.tv_nsec -= 1000000000;
        timeout_ts.tv_sec += 1;
      }
    }

    pthread_mutex_lock(&_mutex);
    _waiters++;

    // Keep track of the current trigger count in a local variable. This is
    // used to guard against spurious wakeups when waiting for the barrier to
    // trigger.
    unsigned int local_trigger_count = _trigger_count;

    if (_waiters >= _capacity)
    {
      // We have reached the required number of waiters so wake up the other
      // threads.
      _trigger_count++;
      _waiters = 0;
      pthread_cond_broadcast(&_cond);
    }

    while (local_trigger_count == _trigger_count)
    {
      // Barrier hasn't been triggered since we first arrived. Wait for it to
      // be triggered.
      if (timeout_us != 0)
      {
        int wait_rc = pthread_cond_timedwait(&_cond, &_mutex, &timeout_ts);

        if (wait_rc == ETIMEDOUT)
        {
          // Timed out. Give up on waiting.
          _waiters--;
          success = false;
          break;
        }
      }
      else
      {
        pthread_cond_wait(&_cond, &_mutex);
      }
    }

    pthread_mutex_unlock(&_mutex);
    return success;
  }

private:
  // The number of threads that must have arrived before the barrier triggers
  // and they all unblock.
  unsigned int _capacity;

  // The number of threads currently waiting for the barrier to trigger.
  unsigned int _waiters;

  // The number of times the barrier has been triggered.
  unsigned int _trigger_count;

  pthread_cond_t _cond;
  pthread_mutex_t _mutex;
};

#endif

