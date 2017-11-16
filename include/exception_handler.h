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

  /// Field containing:
  /// -  The PID of the process that this process has forked to write out a core
  ///    file.
  /// -  Flags used in the management of this field.
  ///
  /// The possible flags are:
  /// -  Lock flag (top bit). If set no other thread should access this field.
  ///    It is used by a thread that wants to dump a core to prevent other
  ///    threads from starting a core dump simultaneously.
  ///
  /// All bits that are not used as flags are used to store the PID of the child
  /// process.
  std::atomic<uint32_t> _core_pid_and_flags;

  const uint32_t PID_LOCK_FLAG = (1 << 31);
  const uint32_t PID_MASK = (uint32_t)(-1) & ~PID_LOCK_FLAG;

  /// Dump a core file. This function ensures that only one core file can be
  /// being dumped at any one time, and does nothing if a core is already being
  /// dumped.
  void dump_one_core();

  /// If a core file has previously been dumped, reap the process, and clean up
  /// internal state to allow a new core to be dumped in future.
  void reap_core_dump_process();
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
