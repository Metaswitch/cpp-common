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

AlarmReqAgent AlarmReqAgent::_instance;

AlarmState::AlarmState(const std::string& issuer,
                       AlarmDef::Index index,
                       AlarmDef::Severity severity) :
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

  AlarmReqAgent::get_instance().alarm_request(req);

  LOG_DEBUG("%s issued %s alarm", _issuer.c_str(), _identifier.c_str());
}

void AlarmState::clear_all(const std::string& issuer)
{
  std::vector<std::string> req;

  req.push_back("clear-alarms");
  req.push_back(issuer);

  AlarmReqAgent::get_instance().alarm_request(req);

  LOG_DEBUG("%s cleared its alarms", issuer.c_str());
}

Alarm::Alarm(const std::string& issuer,
             AlarmDef::Index index,
             AlarmDef::Severity severity) :
  _index(index),
  _clear_state(issuer, index, AlarmDef::CLEARED),
  _set_state(issuer, index, severity),
  _alarmed(false)
{
}

void Alarm::set()
{
  bool previously_alarmed = _alarmed.exchange(true);

  if (!previously_alarmed)
  {
    _set_state.issue();
  }
}

void Alarm::clear()
{
  bool previously_alarmed = _alarmed.exchange(false);

  if (previously_alarmed)
  {
    _clear_state.issue();
  }
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

    LOG_ERROR("AlarmReqAgent: error creating thread %s", strerror(rc));

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
    LOG_DEBUG("AlarmReqAgent: queue overflowed");
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
    LOG_ERROR("AlarmReqAgent: zmq_ctx_new failed: %s", zmq_strerror(errno));
    return false;
  }

  return true;
}

bool AlarmReqAgent::zmq_init_sck()
{
  _sck = zmq_socket(_ctx, ZMQ_REQ);
  if (_sck == NULL)
  {
    LOG_ERROR("AlarmReqAgent: zmq_socket failed: %s", zmq_strerror(errno));
    return false;
  }

  // Configure no linger period so a graceful shutdown will be immediate. It is
  // ok for any pending messages to be dropped as a result of a shutdown.
  int linger = 0;
  if (zmq_setsockopt(_sck, ZMQ_LINGER, &linger, sizeof(linger)) == -1)
  {
    LOG_ERROR("AlarmReqAgent: zmq_setsockopt failed: %s", zmq_strerror(errno));
    return false;
  }

  std::string addr = std::string("ipc:///var/run/clearwater/alarms");
  LOG_DEBUG("AlarmRegAgent: addr=%s", addr.c_str());
  if (zmq_connect(_sck, addr.c_str()) == -1)
  {
    LOG_ERROR("AlarmReqAgent: zmq_connect failed: %s", zmq_strerror(errno));
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
      LOG_ERROR("AlarmReqAgent: zmq_ctx_destroy failed: %s", zmq_strerror(errno));
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
      LOG_ERROR("AlarmReqAgent: zmq_close failed: %s", zmq_strerror(errno));
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
    LOG_DEBUG("AlarmReqAgent: servicing request queue");

    for (std::vector<std::string>::iterator it = req.begin(); it != req.end(); it++)
    {
      if (zmq_send(_sck, it->c_str(), it->size(), ((it + 1) != req.end()) ? ZMQ_SNDMORE : 0) == -1)
      {
        if (errno != ETERM)
        {
          LOG_ERROR("AlarmReqAgent: zmq_send failed: %s", zmq_strerror(errno));
        }

        zmq_clean_sck();
        return;
      }
    }

    if (zmq_recv(_sck, &reply, sizeof(reply), 0) == -1)
    {
      if (errno != ETERM)
      {
        LOG_ERROR("AlarmReqAgent: zmq_recv failed: %s", zmq_strerror(errno));
      }

      zmq_clean_sck();
      return;
    }
  }

  zmq_clean_sck();
}

