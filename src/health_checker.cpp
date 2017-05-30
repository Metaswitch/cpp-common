/**
 * @file health_checker.cpp
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
  int num_recent_passes = _recent_passes.exchange(0);
  if (_hit_exception.load() && (num_recent_passes == 0))
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

void HealthChecker::start_thread()
{
  pthread_create(&_health_check_thread,
                 NULL,
                 &HealthChecker::static_main_thread_function,
                 (void*)this);
 
}
void HealthChecker::stop_thread()
{
  pthread_mutex_lock(&_condvar_lock);
  _terminate = true;
  pthread_cond_signal(&_condvar);
  pthread_mutex_unlock(&_condvar_lock);
  pthread_join(_health_check_thread, NULL);
}
// LCOV_EXCL_STOP
