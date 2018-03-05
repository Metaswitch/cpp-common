/**
 * @file exception_handler.h
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef _EXCEPTION_HANDLER_H__
#define _EXCEPTION_HANDLER_H__

#include <pthread.h>
#include <setjmp.h>
#include <atomic>

#include "health_checker.h"

class ExceptionHandler
{
public:
  /// Constructor
  ExceptionHandler(int ttl,
                   bool attempt_quiesce,
                   HealthChecker* health_checker);

  /// Destructor
  ~ExceptionHandler();

  /// Handle an exception - call longjmp if there's a stored jmp_buf
  void handle_exception();

  /// Create a thread that kills the process after a random time
  void delayed_exit_thread();

private:
  /// Called by a new thread when an exception is hit. Kills the
  /// process after a random time
  static void* delayed_exit_thread_func(void* det);

  /// The delayed exit thread
  pthread_t _delayed_exit_thread;

  /// The maximum time the process should live for
  int _ttl;

  /// Whether the exception handler should attempt to quiesce the process
  bool _attempt_quiesce;

  /// Pointer to the service's health checker
  HealthChecker* _health_checker;

  // Whether we have already dumped a core file (in which case we won't dump any
  // more).
  std::atomic<bool> _dumped_core;

  // Dump a single core file. If this process has already dumped a core file we
  // won't dump another one (to avoid accidentally writing up lots of cores
  // simultaneously in the event of a systematic error).
  void dump_one_core();
};

/// Stored environment
extern pthread_key_t _jmp_buf;

/// TRY Macro
#define CW_TRY                                                                 \
jmp_buf env;                                                                   \
if (setjmp(env) == 0)                                                          \
{                                                                              \
  pthread_setspecific(_jmp_buf, env);

/// EXCEPT Macro
#define CW_EXCEPT(HANDLE_EXCEPTION)                                            \
}                                                                              \
else                                                                           \
{                                                                              \
  /* Spin off waiting thread */                                                \
  HANDLE_EXCEPTION->delayed_exit_thread();

/// END Macro
#define CW_END                                                                 \
  /* Tidy up thread local data */                                              \
  pthread_setspecific(_jmp_buf, NULL);                                         \
  pthread_exit(NULL);                                                          \
}                                                                              \
pthread_setspecific(_jmp_buf, NULL);

#endif
