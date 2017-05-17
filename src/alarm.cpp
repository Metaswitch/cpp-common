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

AlarmState::AlarmState(AlarmReqAgent* alarm_req_agent,
                       const std::string& issuer,
                       const int index,
                       AlarmDef::Severity severity) :
  _alarm_req_agent(alarm_req_agent),
  _issuer(issuer)
{
  _identifier = std::to_string(index) + "." + std::to_string(severity);
}

void AlarmState::issue()
{
  std::vector<std::string> req;
  req.push_back("issue-alarm");
  req.push_back(_issuer);
  req.push_back(_identifier);
  _alarm_req_agent->alarm_request(req);

  TRC_STATUS("%s issued %s alarm", _issuer.c_str(), _identifier.c_str());
}

BaseAlarm::BaseAlarm(AlarmManager* alarm_manager,
                     const std::string& issuer,
                     const int index):
  _index(index),
  _clear_state(alarm_manager->alarm_req_agent(),
               issuer,
               index,
               AlarmDef::CLEARED),
  _last_state_raised(NULL),
  _alarm_manager(alarm_manager)
{
  pthread_mutex_init(&_issue_alarm_change_state, NULL);
  alarm_manager->alarm_re_raiser()->register_alarm(this);
}

BaseAlarm::~BaseAlarm()
{
  _alarm_manager->alarm_re_raiser()->unregister_alarm(this);
  pthread_mutex_destroy(&_issue_alarm_change_state);
}

void BaseAlarm::switch_to_state(AlarmState* new_state)
{
  if (_last_state_raised !=  new_state)
  {
    std::string old_state_text = "NULL";
    std::string new_state_text = "NULL";

    if (_last_state_raised != NULL)
    {
      old_state_text = _last_state_raised->get_identifier();
    }

    if (new_state != NULL)
    {
      new_state_text = new_state->get_identifier();
    }

    pthread_mutex_lock(&_issue_alarm_change_state);
    TRC_STATUS("Alarm severity changed from %s to %s",
               old_state_text.c_str(),
               new_state_text.c_str());
    new_state->issue();
    _last_state_raised = new_state;
    pthread_mutex_unlock(&_issue_alarm_change_state);
  }
}
void BaseAlarm::clear()
{
  switch_to_state(&_clear_state);
}

void BaseAlarm::reraise_last_state()
{
  pthread_mutex_lock(&_issue_alarm_change_state);

  if (_last_state_raised != NULL)
  {
    _last_state_raised->issue();
  }

  pthread_mutex_unlock(&_issue_alarm_change_state);
}

AlarmState::AlarmCondition BaseAlarm::get_alarm_state()
{
  if (_last_state_raised == NULL)
  {
    return AlarmState::UNKNOWN;
  }
  else if (_last_state_raised == &_clear_state)
  {
    return AlarmState::CLEARED;
  }
  else
  {
    return AlarmState::ALARMED;
  }
}

Alarm::Alarm(AlarmManager* alarm_manager,
             const std::string& issuer,
             const int index,
             AlarmDef::Severity severity) :
  BaseAlarm(alarm_manager, issuer, index),
  _set_state(alarm_manager->alarm_req_agent(), issuer, index, severity)
{
}

void Alarm::set()
{
  switch_to_state(&_set_state);
}

MultiStateAlarm::MultiStateAlarm(AlarmManager* alarm_manager,
                                 const std::string& issuer,
                                 const int index) :
  BaseAlarm(alarm_manager, issuer, index),
  _indeterminate_state(alarm_manager->alarm_req_agent(), issuer, index, AlarmDef::INDETERMINATE),
  _warning_state(alarm_manager->alarm_req_agent(), issuer, index, AlarmDef::WARNING),
  _minor_state(alarm_manager->alarm_req_agent(), issuer, index, AlarmDef::MINOR),
  _major_state(alarm_manager->alarm_req_agent(), issuer, index, AlarmDef::MAJOR),
  _critical_state(alarm_manager->alarm_req_agent(), issuer, index, AlarmDef::CRITICAL)
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

AlarmReRaiser::AlarmReRaiser():
  _terminated(false)
{
  // Creates a lock and a condition variable to protect the thread.
  pthread_mutex_init(&_lock, NULL);

#ifdef UNIT_TEST
  _condition = new MockPThreadCondVar(&_lock);
#else
  _condition = new CondVar(&_lock);
#endif

  pthread_create(&_reraising_alarms_thread, NULL, reraise_alarms_function, this);
}

AlarmReRaiser::~AlarmReRaiser()
{
  // Signals the condition variable to terminate the thread.
  pthread_mutex_lock(&_lock);
  _terminated = true;
  _condition->signal();
  pthread_mutex_unlock(&_lock);

  pthread_join(_reraising_alarms_thread, NULL);
  delete _condition; _condition = NULL;
  pthread_mutex_destroy(&_lock);
}

void AlarmReRaiser::register_alarm(BaseAlarm* alarm)
{
  pthread_mutex_lock(&_lock);
  _alarm_list.push_back(alarm);
  pthread_mutex_unlock(&_lock);
}

void AlarmReRaiser::unregister_alarm(BaseAlarm* alarm)
{
  pthread_mutex_lock(&_lock);
  std::vector<BaseAlarm*>::iterator it = std::find(_alarm_list.begin(), _alarm_list.end(), alarm);
  if (it != _alarm_list.end())
  {
    _alarm_list.erase(it);
  }
  pthread_mutex_unlock(&_lock);
}

// This function runs on the thread created by the AlarmReRaiser constructor. To
// be compatible with the pthread this function needs to accept and return a
// void pointer. This void pointer is then cast to an AlarmManger pointer in order to
// call the reraise_alarms method.
void* AlarmReRaiser::reraise_alarms_function(void* data)
{
  ((AlarmReRaiser*)data)->reraise_alarms();
  return NULL;
}

void AlarmReRaiser::reraise_alarms()
{
  TRC_DEBUG("Started reraising alarms every 30 seconds");
  pthread_mutex_lock(&_lock);

  while (!_terminated)
  {
    // Sets the limit for when we want the thread to wake up and start
    // re-issuing alarms again.
    struct timespec time_limit;
    clock_gettime(CLOCK_MONOTONIC, &time_limit);
    time_limit.tv_sec += 30;

    // Now raise the alarms
    TRC_STATUS("Reraising all alarms with a known state");
    for (std::vector<BaseAlarm*>::iterator it = _alarm_list.begin();
                                           it != _alarm_list.end();
                                           it++)
    {
      (*it)->reraise_last_state();
    }

    // Wait if it took less than 30 seconds to raise the alarms. If the time has
    // already passed, then this instantly wakes up (as the timedwait call fails
    // with ETIMEDOUT). After waiting, get the timespec for 30 secs time.
    _condition->timedwait(&time_limit);
  }

  pthread_mutex_unlock(&_lock);
  TRC_INFO("Thread to reraise alarms terminating");
}

AlarmReqAgent::AlarmReqAgent() : _ctx(NULL), _sck(NULL), _req_q(NULL)
{
  _req_q = new eventq<std::vector<std::string> >(MAX_Q_DEPTH);

  pthread_mutex_init(&_start_mutex, NULL);
  pthread_cond_init(&_start_cond, NULL);
  pthread_mutex_lock(&_start_mutex);

  int rc = pthread_create(&_thread, NULL, &agent_thread, (void*)this);
  if (rc == 0)
  {
    // This blocks until the ZMQ thread is ready to accept incoming messages
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
    delete _req_q; _req_q = NULL;
    // LCOV_EXCL_STOP
  }
}

AlarmReqAgent::~AlarmReqAgent()
{
  _req_q->terminate();
  zmq_clean_ctx();
  pthread_join(_thread, NULL);
  delete _req_q; _req_q = NULL;
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

#ifdef UNIT_TEST
  std::string addr = "ipc:///tmp/ut-alarms-socket-" + std::to_string(getpid());
#else
  std::string addr = "ipc:///var/run/clearwater/alarms";
#endif

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
  bool zmqOk = zmq_init_ctx();
  if (zmqOk)
  {
    zmqOk = zmq_init_sck();
  }

  pthread_mutex_lock(&_start_mutex);
  pthread_cond_signal(&_start_cond);
  pthread_mutex_unlock(&_start_mutex);

  if (!zmqOk)
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

