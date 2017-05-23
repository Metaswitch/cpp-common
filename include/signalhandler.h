/**
 * @file signalhandler.h  Handler for UNIX signals.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef SIGNALHANDLER_H__
#define SIGNALHANDLER_H__

#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include "log.h"

#include <time.h>
#include <sys/time.h>

// Interface that should be implemented by any object that can be used to wait
// on a signal. This is to allow code to wait on different signals at
// runtime (which would otherwise not be possible since SignalHandler is
// templated).
class SignalWaiter
{
public:
  /// Waits for the signal to be raised, or for the wait to timeout.
  /// This returns true if the signal was raised, and false for timeout.
  virtual bool wait_for_signal() = 0;
};

/// Singleton class template for handling UNIX signals.  Only a single
/// instance of this class should be created for each UNIX signal type.
/// This has to be templated because there has to be a unique static signal
/// handler function and semaphore for each signal being hooked, so creating
/// multiple instances of a non-templated class doesn't work

template <int SIGNUM>
class SignalHandler : public SignalWaiter
{
public:
  SignalHandler()
  {
    // Create the mutex and condition.
    pthread_mutex_init(&_mutex, NULL);
    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
    pthread_cond_init(&_cond, &cond_attr);
    pthread_condattr_destroy(&cond_attr);

    // Create the semaphore
    sem_init(&_sema, 0, 0);
  }

  ~SignalHandler()
  {
    // Unhook the signal.
    signal(SIGNUM, SIG_DFL);

    // Cancel the dispatcher thread and wait for it to end.
    pthread_cancel(_dispatcher_thread);
    pthread_join(_dispatcher_thread, NULL);

    // Destroy the semaphore.
    sem_destroy(&_sema);

    // Destroy the mutex and condition.
    pthread_cond_destroy(&_cond);
    pthread_mutex_destroy(&_mutex);
  }

  void start()
  {
    // Create the dispatcher thread.
    pthread_create(&_dispatcher_thread, 0, &SignalHandler::dispatcher, (void*)this);

    // Hook the signal.
    sighandler_t old_handler = signal(SIGNUM, &SignalHandler::handler);

    if (old_handler != SIG_DFL)
    {
// LCOV_EXCL_START
      // Old handler is not the default handler, so someone else has previously
      // hooked the signal.
      TRC_WARNING("SIGNAL already hooked");
// LCOV_EXCL_STOP
    }
  }

  bool wait_for_signal()
  {
    // Grab the mutex.  On its own this isn't enough to guarantee we won't
    // miss a signal, but to do that we would have to hold the mutex while
    // calling back to user code, which is not desirable.  If we really
    // cannot miss signals then we will probably need to add sequence numbers
    // to this API.
    pthread_mutex_lock(&_mutex);

    // Wait for either the signal condition to trigger or timeout
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
#ifndef UNIT_TEST
    ts.tv_sec += 1;
#else
    // In unit tests we have to wait for this timedwait to finish in
    // several destructors, so want it to finish faster (1ms) at the
    // expense of being less efficient.
    ts.tv_nsec += 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }
#endif

    int rc = pthread_cond_timedwait(&_cond, &_mutex, &ts);

    // Unlock the mutex
    pthread_mutex_unlock(&_mutex);

    return (rc != ETIMEDOUT);
  }

private:
  /// Thread responsible for dispatching signals to the appropriate caller.
  static void* dispatcher(void* p)
  {
    while (true)
    {
      // Wait for the signal handler to indicate the signal has been raised.
      sem_wait(&_sema);
      TRC_DEBUG("Signal %d raised", SIGNUM);

      // Broadcast to all the waiting threads.
      pthread_cond_broadcast(&_cond);
    }
    return NULL;
  }

  /// The signal handler.
  static void handler(int sig)
  {
    // Post the semaphore to wake up the dispatcher.
    sem_post(&_sema);
  }

  /// Identifier of dispatcher thread.
  pthread_t _dispatcher_thread;

  /// Mutex used for signalling to waiting threads.
  static pthread_mutex_t _mutex;

  /// Condition used for signalling to waiting threads.
  static pthread_cond_t _cond;

  /// Semaphore used for signaling from signal handler to dispatcher thread.
  static sem_t _sema;
};

template<int SIGNUM> pthread_mutex_t SignalHandler<SIGNUM>::_mutex;
template<int SIGNUM> pthread_cond_t SignalHandler<SIGNUM>::_cond;
template<int SIGNUM> sem_t SignalHandler<SIGNUM>::_sema;

// Concrete instances of signal handers
extern SignalHandler<SIGHUP> _sighup_handler;
extern SignalHandler<SIGUSR1> _sigusr1_handler;
extern SignalHandler<SIGUSR2> _sigusr2_handler;

// This starts the signal handlers. This creates a new thread for each
// handler, so this function must not be called before the process has
// daemonised (if it's going to)
inline void start_signal_handlers()
{
  _sighup_handler.start();
  _sigusr1_handler.start();
  _sigusr2_handler.start();
}

#endif
