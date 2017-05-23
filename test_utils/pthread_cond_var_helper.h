/**
 * @file pthread_cond_var_helper.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef PTHREAD_COND_VAR_HELPER_H__
#define PTHREAD_COND_VAR_HELPER_H__

#include <pthread.h>

enum STATE { WAIT, TIMED_WAIT, SIGNALED, TIMEDOUT };

class MockPThreadCondVar
{
public:
  MockPThreadCondVar(pthread_mutex_t *mutex);
  ~MockPThreadCondVar();

  // Mirror of CondVar
  int wait();
  int timedwait(struct timespec*);
  int signal();

  // Test script functions
  bool check_signaled();
  void block_till_waiting();
  void block_till_signaled();
  void check_timeout(const struct timespec&);
  void lock();
  void unlock();
  void signal_wake();
  void signal_timeout();

private:

  STATE _state;
  struct timespec _timeout;
  bool _signaled;
  pthread_mutex_t* _mutex;
  pthread_cond_t _cond;
};

#endif
