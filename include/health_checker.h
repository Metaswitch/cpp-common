/**
 * @file health_checker.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef HEALTH_CHECKER_H
#define HEALTH_CHECKER_H

#include <atomic>
#include <pthread.h>

// Health-checking object which:
//  - is notified when "healthy behaviour" happens (e.g. a 200 OK response)
//  - is notified when an exception is hit
//  - checks every 60 seconds to see if an exception has been hit and
//    no healthy behaviour has been seen since the last check, and
//    aborts the process if so.
class HealthChecker
{
public:
  HealthChecker();
  virtual ~HealthChecker();

  // Virtual for mocking in UT
  virtual void health_check_passed();
  void hit_exception();
  void do_check();
  void start_thread();
  void stop_thread();

  static void* static_main_thread_function(void* health_checker);
  void main_thread_function();
  
private:
  std::atomic_int _recent_passes;
  std::atomic_bool _hit_exception;
  std::atomic_bool _terminate;
  pthread_cond_t _condvar;
  pthread_mutex_t _condvar_lock;
  pthread_t _health_check_thread;
};

#endif
