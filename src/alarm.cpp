/**
 * @file alarm.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include <string.h>
#include <time.h>
#include <zmq.h>

#include "alarm.h"
#include "log.h"

// When an alarm is issued we change the _last_state_raised member variable of
// the alarm. We must not allow another thread to raise the same alarm at a
// different severity between these operations for this would cause data
// contention. To ensure we don't do this the call to issue the alarm and change
// the _last_state_raised are protected by this lock.
pthread_mutex_t issue_alarm_change_state;

AlarmReqAgent AlarmReqAgent::_instance;
std::unique_ptr<AlarmManager> AlarmManager::_instance;
pthread_once_t AlarmManager::alarm_manager_singleton_once = PTHREAD_ONCE_INIT;

AlarmState::AlarmState(const std::string& issuer,
                       const int index,
                       AlarmDef::Severity severity) :
  _issuer(issuer)
{
  _identifier = std::to_string(index) + "." + std::to_string(severity);
}

void AlarmState::issue()
{
  AlarmManager::get_instance().start_resending_alarms();
  std::vector<std::string> req;

  req.push_back("issue-alarm");
  req.push_back(_issuer);
  req.push_back(_identifier);
  AlarmReqAgent::get_instance().alarm_request(req);
  TRC_DEBUG("%s issued %s alarm", _issuer.c_str(), _identifier.c_str());
}

void AlarmState::clear_all(const std::string& issuer)
{
  std::vector<std::string> req;

  req.push_back("clear-alarms");
  req.push_back(issuer);

  AlarmReqAgent::get_instance().alarm_request(req);

  TRC_DEBUG("%s cleared its alarms", issuer.c_str());
}

BaseAlarm::BaseAlarm(const std::string& issuer,
                     const int index):
  _index(index),
  _clear_state(issuer, index, AlarmDef::CLEARED),
  _last_state_raised(NULL)
{
  AlarmManager::get_instance().register_alarm(this);
}

void BaseAlarm::switch_to_state(AlarmState* new_state)
{
  if (_last_state_raised !=  new_state)
  {
    pthread_mutex_lock (&issue_alarm_change_state);
    new_state->issue();
    _last_state_raised = new_state;
    pthread_mutex_unlock (&issue_alarm_change_state);
  }
}
void BaseAlarm::clear()
{
  switch_to_state(&_clear_state);
}

void BaseAlarm::reraise_last_state()
{
  if (_last_state_raised != NULL)
  {
    _last_state_raised->issue();
  }
}

Alarm::Alarm(const std::string& issuer,
             const int index,
             AlarmDef::Severity severity) :
  BaseAlarm(issuer, index),
  _set_state(issuer, index, severity)
{
}

void Alarm::set()
{
  switch_to_state(&_set_state);
}

MultiStateAlarm::MultiStateAlarm(const std::string& issuer,
                                 const int index) :
  BaseAlarm(issuer, index),
  _indeterminate_state(issuer, index, AlarmDef::INDETERMINATE),
  _warning_state(issuer, index, AlarmDef::WARNING),
  _minor_state(issuer, index, AlarmDef::MINOR),
  _major_state(issuer, index, AlarmDef::MAJOR),
  _critical_state(issuer, index, AlarmDef::CRITICAL)
{
}

// We don't have any UTs that use an indeterminate state.
// LCOV_EXCL_START
void MultiStateAlarm::set_indeterminate()
{
  switch_to_state(&_indeterminate_state);
}
// LCOV_EXCL_STOP

void MultiStateAlarm::set_warning()
{
  switch_to_state(&_warning_state);
}

void MultiStateAlarm::set_minor()
{
  switch_to_state(&_minor_state);
}

void MultiStateAlarm::set_major()
{
  switch_to_state(&_major_state);
}

void MultiStateAlarm::set_critical()
{
  switch_to_state(&_critical_state);
}

void AlarmManager::create_singleton()
{
  _instance = std::unique_ptr<AlarmManager>(new AlarmManager());
}

AlarmManager& AlarmManager::get_instance()
{
  pthread_once(&alarm_manager_singleton_once, AlarmManager::create_singleton);
  return *_instance;
}

AlarmManager::AlarmManager():
  _terminated(false),
  _first_alarm_raised(false)
{
  // Creates a lock and a condition variable to protect the thread.
  pthread_mutex_init(&_lock, NULL);
  pthread_condattr_t cond_attr;
  pthread_condattr_init(&cond_attr);
  pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
  pthread_cond_init(&_condition, &cond_attr);
  pthread_condattr_destroy(&cond_attr);
  pthread_create(&_reraising_alarms_thread, NULL, reraise_alarms_function, this);
}

AlarmManager::~AlarmManager()
{
  pthread_mutex_lock(&_lock);
  _terminated = true;
  // Signals the condition variable to terminate the thread.
  pthread_cond_signal(&_condition);
  pthread_mutex_unlock(&_lock);
  pthread_join(_reraising_alarms_thread, NULL);
  pthread_cond_destroy(&_condition);
  pthread_mutex_destroy(&_lock);
}

void AlarmManager::register_alarm(BaseAlarm* alarm)
{
  pthread_mutex_lock(&_lock);
  _alarm_list.push_back(alarm);
  pthread_mutex_unlock(&_lock);
}

// This function runs on the thread created by the AlarmManager constructor. To
// be compatible with the pthread this function needs to accept and return a
// void pointer. This void pointer is then cast to an AlarmManger pointer in order to
// call the reraise_alarms method.
void* AlarmManager::reraise_alarms_function(void* data)
{
  ((AlarmManager*)data)->reraise_alarms();
  return NULL;
}

void AlarmManager::reraise_alarms()
{
  TRC_DEBUG("Started reraising alarms every 30 seconds");
  struct timespec time_limit;
#ifdef UNIT_TEST
  int UT_TIME_BETWEEN_CHECKS = 1000;
#endif
  pthread_mutex_lock(&_lock);
  clock_gettime(CLOCK_MONOTONIC, &time_limit);
  
  while (!_terminated)
  {
    // Sets the limit for when we want the thread to wake up and start
    // re-issueing alarms again. When we are Unit Testing with Valgrind we need
    // to ensure the UTs have enough time to complete before we start resending
    // alarms, so we add 1000s on to the time limit used below and then in UTs
    // we can simulate time moving forward by 1000s if we want to trigger alarms
    // being resent. 
#ifdef UNIT_TEST
    time_limit.tv_sec += UT_TIME_BETWEEN_CHECKS;
#else
    time_limit.tv_sec += 30;
#endif
    if (_first_alarm_raised)
    {
      TRC_DEBUG("Reraising alarms");
      for (std::vector<BaseAlarm*>::iterator it = _alarm_list.begin(); it != _alarm_list.end(); it++)
      {
        (*it)->reraise_last_state();
      }
    }

    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    
    // Forces us to wait if it took less than 30 seconds to raise the alarms.
    while (((current_time.tv_sec < time_limit.tv_sec && !_terminated) ||
            ((current_time.tv_sec == time_limit.tv_sec) && (current_time.tv_nsec < time_limit.tv_nsec))) &&
            !_terminated)
    {
      // When we are unit testing this function we want to sleep in 10ms
      // increments. This gives the UT a chance to simulate 1000 seconds of 
      // time passing to cause all the alarms to be re-raised. We have to make
      // sure though that adding on 10ms doesn't cause tv_nsec to go over its
      // limit of a billion nanoseconds, in this case we incrament the second
      // counter.
#ifdef UNIT_TEST
      time_limit.tv_nsec += 10 * 1000 * 1000;
      if (time_limit.tv_nsec >= (1000 * 1000 * 1000))
      {
        // LCOV_EXCL_START
        time_limit.tv_nsec -= 1000 * 1000 * 1000;
        time_limit.tv_sec += 1;
        // LCOV_EXCL_STOP
      }
      // We want to temporarily reduce the time_limit for UTs to 10ms, this
      // allows us to pass pthread_cond_timedwait quickly but keeps us in this
      // while loop until we manually advance time by 1000s in a UT.
      time_limit.tv_sec -= UT_TIME_BETWEEN_CHECKS;
#endif
      pthread_cond_timedwait(&_condition, &_lock, &time_limit);
      clock_gettime(CLOCK_MONOTONIC, &current_time);
#ifdef UNIT_TEST
      time_limit.tv_sec += UT_TIME_BETWEEN_CHECKS;
#endif
    }
  }
  pthread_mutex_unlock(&_lock);
  TRC_INFO("Reraising alarms thread terminating");
}

bool AlarmReqAgent::start()
{
  if (!zmq_init_ctx())
  {
    return false;
  }

  _req_q = new eventq<std::vector<std::string> >(MAX_Q_DEPTH);

  pthread_mutex_init(&_start_mutex, NULL);
  pthread_cond_init(&_start_cond, NULL);

  pthread_mutex_lock(&_start_mutex);

  int rc = pthread_create(&_thread, NULL, &agent_thread, (void*) &_instance);
  if (rc == 0)
  {
    pthread_cond_wait(&_start_cond, &_start_mutex);
  }

  pthread_mutex_unlock(&_start_mutex);

  pthread_cond_destroy(&_start_cond);
  pthread_mutex_destroy(&_start_mutex);

  if (rc != 0)
  {
    // LCOV_EXCL_START - No mock for pthread_create

    TRC_ERROR("AlarmReqAgent: error creating thread %s", strerror(rc));

    zmq_clean_ctx();

    delete _req_q;
    _req_q = NULL;

    return false;

    // LCOV_EXCL_STOP
  }

  return true;
}

void AlarmReqAgent::stop()
{
  _req_q->terminate();

  zmq_clean_ctx();

  pthread_join(_thread, NULL);

  delete _req_q;
  _req_q = NULL;
}

void AlarmReqAgent::alarm_request(std::vector<std::string> req)
{
  if (!_req_q->push_noblock(req))
  {
    TRC_DEBUG("AlarmReqAgent: queue overflowed");
  }
}

void* AlarmReqAgent::agent_thread(void* alarm_req_agent)
{
  ((AlarmReqAgent*) alarm_req_agent)->agent();

  return NULL;
}

AlarmReqAgent::AlarmReqAgent() : _ctx(NULL), _sck(NULL), _req_q(NULL)
{
}

bool AlarmReqAgent::zmq_init_ctx()
{
  _ctx = zmq_ctx_new();
  if (_ctx == NULL)
  {
    TRC_ERROR("AlarmReqAgent: zmq_ctx_new failed: %s", zmq_strerror(errno));
    return false;
  }

  return true;
}

bool AlarmReqAgent::zmq_init_sck()
{
  _sck = zmq_socket(_ctx, ZMQ_REQ);
  if (_sck == NULL)
  {
    TRC_ERROR("AlarmReqAgent: zmq_socket failed: %s", zmq_strerror(errno));
    return false;
  }

  // Configure no linger period so a graceful shutdown will be immediate. It is
  // ok for any pending messages to be dropped as a result of a shutdown.
  int linger = 0;
  if (zmq_setsockopt(_sck, ZMQ_LINGER, &linger, sizeof(linger)) == -1)
  {
    TRC_ERROR("AlarmReqAgent: zmq_setsockopt failed: %s", zmq_strerror(errno));
    return false;
  }

  std::string addr = "ipc:///var/run/clearwater/alarms";

  if (zmq_connect(_sck, addr.c_str()) == -1)
  {
    TRC_ERROR("AlarmReqAgent: zmq_connect failed: %s", zmq_strerror(errno));
    return false;
  }

  return true;
}

void AlarmReqAgent::zmq_clean_ctx()
{
  if (_ctx)
  {
    if (zmq_ctx_destroy(_ctx) == -1)
    {
      TRC_ERROR("AlarmReqAgent: zmq_ctx_destroy failed: %s", zmq_strerror(errno));
    }

    _ctx = NULL;
  }
}

void AlarmReqAgent::zmq_clean_sck()
{
  if (_sck)
  {
    if (zmq_close(_sck) == -1)
    {
      TRC_ERROR("AlarmReqAgent: zmq_close failed: %s", zmq_strerror(errno));
    }

    _sck = NULL;
  }
}

void AlarmReqAgent::agent()
{
  bool sckOk = zmq_init_sck();

  pthread_mutex_lock(&_start_mutex);
  pthread_cond_signal(&_start_cond);
  pthread_mutex_unlock(&_start_mutex);

  if (!sckOk)
  {
    return;
  }

  std::vector<std::string> req;

  char reply[MAX_REPLY_LEN];

  while (_req_q && _req_q->pop(req))
  {
    TRC_DEBUG("AlarmReqAgent: servicing request queue");

    for (std::vector<std::string>::iterator it = req.begin(); it != req.end(); it++)
    {
      if (zmq_send(_sck, it->c_str(), it->size(), ((it + 1) != req.end()) ? ZMQ_SNDMORE : 0) == -1)
      {
        if (errno != ETERM)
        {
          TRC_ERROR("AlarmReqAgent: zmq_send failed: %s", zmq_strerror(errno));
        }

        zmq_clean_sck();
        return;
      }
    }

    if (zmq_recv(_sck, &reply, sizeof(reply), 0) == -1)
    {
      if (errno != ETERM)
      {
        TRC_ERROR("AlarmReqAgent: zmq_recv failed: %s", zmq_strerror(errno));
      }

      zmq_clean_sck();
      return;
    }
  }

  zmq_clean_sck();
}

