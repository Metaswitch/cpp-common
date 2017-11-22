/**
 * @file exception_handler.cpp
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <pthread.h>
#include <setjmp.h>
#include <unistd.h>
#include <cstdlib>
#include <signal.h>

#include "exception_handler.h"
#include "health_checker.h"
#include "log.h"

pthread_key_t _jmp_buf;

ExceptionHandler::ExceptionHandler(int ttl,
                                   bool attempt_quiesce,
                                   HealthChecker* health_checker) :
  _ttl(ttl),
  _attempt_quiesce(attempt_quiesce),
  _health_checker(health_checker),
  _dumped_core(false)
{
  pthread_key_create(&_jmp_buf, NULL);
}

ExceptionHandler::~ExceptionHandler()
{
  pthread_key_delete(_jmp_buf);
}

void ExceptionHandler::handle_exception()
{
  // Check if there's a stored jmp_buf on the thread and handle if there is
  jmp_buf* env = (jmp_buf*)pthread_getspecific(_jmp_buf);

  if (env != NULL)
  {
    dump_one_core();

    // Let the health check know that an exception has occurred
    _health_checker->hit_exception();

    // Jump back to the stored state
    longjmp(*env, 1);
  }
}

void ExceptionHandler::delayed_exit_thread()
{
  pthread_create(&_delayed_exit_thread,
                 NULL,
                 delayed_exit_thread_func,
                 (void*)this);
  pthread_detach(_delayed_exit_thread);
}

void* ExceptionHandler::delayed_exit_thread_func(void* det)
{
  // Wait for a random time up to the _ttl. This thread was detached when it
  // was created, so we can safely call sleep
  int sleep_time = rand() % ((ExceptionHandler*)det)->_ttl;
  TRC_WARNING("Delayed exit will shutdown this process in %d seconds", sleep_time);
  sleep(sleep_time);

  // Raise a SIGQUIT if needed.
  if (((ExceptionHandler*)det)->_attempt_quiesce)
  {
    TRC_WARNING("Delayed exit attempting to quiesce process");
    raise(SIGQUIT);
    sleep(10);
  }

  TRC_WARNING("Delayed exit shutting down process");
  exit(1);
}

void ExceptionHandler::dump_one_core()
{
  // Only dump a core if:
  // - We've not dumped one already.
  // - We can CAS the dumped core flag to true. If we can't another thread must
  //   have beaten us to it.
  bool dumped_core = _dumped_core.load();

  if (!dumped_core && _dumped_core.compare_exchange_strong(dumped_core, true))
  {
    TRC_STATUS("Dumping core file");

    if (!fork())
    {
      abort();
    }
  }
  else
  {
    TRC_STATUS("Not dumping core file - core has already been dumped for this process");
  }
}
