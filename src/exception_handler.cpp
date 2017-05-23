/**
 * @file exception_handler.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
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

pthread_key_t _jmp_buf;

ExceptionHandler::ExceptionHandler(int ttl, 
                                   bool attempt_quiesce,
                                   HealthChecker* health_checker) :
  _ttl(ttl),
  _attempt_quiesce(attempt_quiesce),
  _health_checker(health_checker)
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
    // Create a child thread, then abort it to create a core file.
    if (!fork())
    {
      abort();
    }

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
  sleep(sleep_time);

  // Raise a SIGQUIT if needed. 
  if (((ExceptionHandler*)det)->_attempt_quiesce)
  {
    raise(SIGQUIT);
    sleep(10);
  }

  exit(1);
}
