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
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>

#include "exception_handler.h"
#include "health_checker.h"
#include "log.h"

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
    // Try to reap any child core dumping process.
    reap_core_dump_process();

    // Dump a core file (if we're not already dumping one).
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
  sleep(sleep_time);

  // Raise a SIGQUIT if needed.
  if (((ExceptionHandler*)det)->_attempt_quiesce)
  {
    raise(SIGQUIT);
    sleep(10);
  }

  exit(1);
}

void ExceptionHandler::dump_one_core()
{
  uint32_t core_pid_and_flags = _core_pid_and_flags.load();

  if (((core_pid_and_flags & PID_LOCK_FLAG) == 0) && ((core_pid_and_flags & PID_MASK) == 0))
  {
    // The core PID value is not locked, nor is a core currently being dumped.
    // Attempt to dump one.

    if (_core_pid_and_flags.compare_exchange_strong(core_pid_and_flags, PID_LOCK_FLAG))
    {
      // We successfully locked the PID variable. We can now safely dump the
      // core.
      int fork_rc = fork();

      if (fork_rc < 0)
      {
        // Fork failed. Give up and unlock the PID variable.
        TRC_WARNING("Unable to fork to create core file. Error: %s", strerror(errno));
        _core_pid_and_flags.store(0);
      }
      else if (fork_rc == 0)
      {
        // We're the child. Abort to generate a core file.
        abort();
      }
      else
      {
        // We're the parent. Record the child's PID and unlock the value. No
        // need to do a CAS as we already have the PID variable locked.
        TRC_STATUS("Dumping core file using PID %lu", fork_rc);
        _core_pid_and_flags.store(fork_rc);
      }
    }
    else
    {
      TRC_STATUS("Not dumping core, as another thread has triggered one");
    }
  }
  else
  {
    TRC_STATUS("Not dumping core file as one is being dumped, or is about to be dumped. "
               "(PID locked = %s, child PID = %lu)",
               ((core_pid_and_flags & PID_LOCK_FLAG) == 0) ? "false" : "true",
               (core_pid_and_flags & PID_MASK));
  }
}

void ExceptionHandler::reap_core_dump_process()
{
  uint32_t core_pid_and_flags = _core_pid_and_flags.load();

  if (((core_pid_and_flags & PID_LOCK_FLAG) == 0) && ((core_pid_and_flags & PID_MASK) != 0))
  {
    int pid = core_pid_and_flags & PID_MASK;

    // Check if the core process has exited by doing a non-blocking wait.
    int unused_status;
    int rc = waitpid(pid, &unused_status, WNOHANG);

    if (rc < 0)
    {
      TRC_STATUS("Core process %d does not exist - it has probably already been reaped", pid);
    }
    else if (rc > 0)
    {
      // The process has exited, so clear the PID. Do a CAS here to avoid the
      // situation where:
      // - Thread 1 and thread 2 are trying to reap the child.
      // - Thread 3 is about to dump a core.
      // - Thread 1 stores 0 in _core_pid_and_flags.
      // - Thread 3 starts dumping the core and stores the child in _core_pid_and_flags.
      // - Thread 2 overwrites _core_pid_and_flags even though there is an active child.
      _core_pid_and_flags.compare_exchange_strong(core_pid_and_flags, 0);
      TRC_STATUS("Core process %d has terminated", pid);
    }
  }
}
