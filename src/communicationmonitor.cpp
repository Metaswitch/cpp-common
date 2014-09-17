/**
 * @file communicationmonitor.cpp
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

#include "communicationmonitor.h"
#include "log.h"


CommunicationMonitor::CommunicationMonitor(const std::string& issuer,
                                           const std::string& clear_alarm_id,
                                           const std::string& set_alarm_id,
                                           unsigned int clear_confirm_sec,
                                           unsigned int set_confirm_sec) :
  _alarms(issuer, clear_alarm_id, set_alarm_id),
  _clear_confirm_ms(clear_confirm_sec * 1000),
  _set_confirm_ms(set_confirm_sec * 1000),
  _sent(0), _failed(0)
{
  _next_check = current_time_ms() + _set_confirm_ms;

  pthread_mutex_init(&_lock, NULL);
}


CommunicationMonitor::~CommunicationMonitor()
{
  pthread_mutex_destroy(&_lock);
}


void CommunicationMonitor::inform_success(unsigned long now_ms)
{
  __sync_fetch_and_add(&_sent, 1);

  update_alarm_state(now_ms);
}


void CommunicationMonitor::inform_failure(unsigned long now_ms)
{
  __sync_fetch_and_add(&_failed, 1);

  update_alarm_state(now_ms);
}


void CommunicationMonitor::update_alarm_state(unsigned long now_ms)
{
  now_ms = now_ms ? now_ms : current_time_ms();

  if (now_ms > _next_check)
  {
    // LCOV_EXCL_START - no UT for check cycle 

    pthread_mutex_lock(&_lock);

    if (now_ms > _next_check)
    {
      unsigned int sent   = __sync_fetch_and_and(&_sent,   0);
      unsigned int failed = __sync_fetch_and_and(&_failed, 0);

      if (! _alarms.alarmed())
      {
        if ((sent == 0) && (failed != 0))
        {
          _alarms.set();
        }
      }
      else
      {
        if (sent != 0)
        {
          _alarms.clear();
        }
      }

      _next_check = (_alarms.alarmed()) ? now_ms + _clear_confirm_ms :
                                          now_ms + _set_confirm_ms   ;
    }

    pthread_mutex_unlock(&_lock);

    // LCOV_EXCL_STOP
  }
}


unsigned long CommunicationMonitor::current_time_ms()
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);

  return ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
}

