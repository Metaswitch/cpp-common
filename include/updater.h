/**
 * @file updater.h Declarations for Updater class.
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef UPDATER_H__
#define UPDATER_H__

#include "signalhandler.h"
#include <pthread.h>
#include <functional>

template<class ReturnType, class ClassType> class Updater
{
public:
  Updater(ClassType* pointer,
          std::mem_fun_t<ReturnType, ClassType> myFunctor,
          SignalWaiter* signal_waiter = &_sighup_handler,
          bool run_on_start = true) :
    _terminate(false),
    _func(myFunctor),
    _pointer(pointer),
    _signal_waiter(signal_waiter)
  {
    TRC_DEBUG("Created updater");

    // Do initial configuration.
    if (run_on_start)
    {
      myFunctor(pointer);
    }

    // Create the thread to handle further changes of view.
    int rc = pthread_create(&_updater, NULL, &updater_thread, this);

    if (rc < 0)
    {
      // LCOV_EXCL_START
      TRC_ERROR("Error creating updater thread");
      // LCOV_EXCL_STOP
    }
  }

  ~Updater()
  {
    // Destroy the updater thread.
    _terminate = true;
    pthread_join(_updater, NULL);
  }

private:
  static void* updater_thread(void* p)
  {
    ((Updater*)p)->updater();
    return NULL;
  }

  void updater()
  {
    TRC_DEBUG("Started updater thread");

    while (!_terminate)
    {
      // Wait for the SIGHUP signal.
      bool rc = _signal_waiter->wait_for_signal();

      // If the signal handler didn't timeout, then call the
      // update function
      if (rc)
      {
        // LCOV_EXCL_START
        _func(_pointer);
        // LCOV_EXCL_STOP
      }
    }
  }

  volatile bool _terminate;
  std::mem_fun_t<ReturnType, ClassType> _func;
  pthread_t _updater;
  ClassType* _pointer;
  SignalWaiter* _signal_waiter;
};

#endif
