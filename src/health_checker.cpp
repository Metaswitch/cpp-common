/**
 * @file health_checker.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2015 Metaswitch Networks Ltd
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


#include <cassert>
#include "time.h"

#include "health_checker.h"
#include "log.h"

HealthChecker::HealthChecker() :
  _recent_passes(0),
  _hit_exception(false),
  _terminate(false)
{
  _condvar = PTHREAD_COND_INITIALIZER;
  _condvar_lock = PTHREAD_MUTEX_INITIALIZER; 

  pthread_condattr_t cond_attr;
  pthread_condattr_init(&cond_attr);
  pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
  pthread_cond_init(&_condvar, &cond_attr);
  pthread_condattr_destroy(&cond_attr);
}

HealthChecker::~HealthChecker()
{
  pthread_cond_destroy(&_condvar);
  pthread_mutex_destroy(&_condvar_lock);
}

void HealthChecker::hit_exception()
{
  _hit_exception = true;
}

// Logging from the HealthChecker thread is a bad idea in general:
// the log functions take locks, to ensure that only one thread
// writes to a file at a time. If we hit a segfault on a thread
// holding that lock, any logging call could block forever.

// Useful logs are included, but commented out, for use during testing.
void HealthChecker::health_check_passed()
{
  _recent_passes++;
  // TRC_DEBUG("Health check passed, %d passes in last 60s", _recent_passes.load());
}

// Checking function which should be run every 60s on a thread. Aborts
// the process if we have hit an exception and health_check_passed()
// has not been called since this was last run.
void HealthChecker::do_check()
{
  if (_hit_exception.load() && (_recent_passes.load() == 0))
  {
    // LCOV_EXCL_START - only covered in "death tests"

    // TRC_ERROR("Check for overall system health failed - exiting");
    exit(1);
    // LCOV_EXCL_STOP
  }
  else
  {
    // TRC_DEBUG("Check for overall system health passed - not exiting");
  }
  _recent_passes = 0;
}

// LCOV_EXCL_START
void* HealthChecker::static_main_thread_function(void* health_checker)
{
    ((HealthChecker*)health_checker)->main_thread_function();
    return NULL;
}

void HealthChecker::main_thread_function()
{
  struct timespec end_wait;
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  end_wait.tv_sec = now.tv_sec + 60;
  end_wait.tv_nsec = now.tv_nsec;
  pthread_mutex_lock(&_condvar_lock);
  while (true)
  {
    pthread_cond_timedwait(&_condvar, &_condvar_lock, &end_wait);

    // If we wake up and find the terminate flag is set, don't run a
    // check and break out of the infinite loop
    if (_terminate)
    {
      break;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &now);

    // If pthread_cond_timedwait waited for the correct amount of
    // time, run a check and advance the end time.
    if ((now.tv_sec > end_wait.tv_sec) || ((now.tv_sec == end_wait.tv_sec) && (now.tv_nsec >= end_wait.tv_nsec)))
    {
      do_check();
      end_wait.tv_sec += 60;
    }
  }
  pthread_mutex_unlock(&_condvar_lock);
}

void HealthChecker::terminate()
{
  pthread_mutex_lock(&_condvar_lock);
  _terminate = true;
  pthread_cond_signal(&_condvar);
  pthread_mutex_unlock(&_condvar_lock);
}
// LCOV_EXCL_STOP
