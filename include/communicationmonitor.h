/**
 * @file communicationmonitor.h
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

#ifndef COMMUNICATION_MAONITOR_H__
#define COMMUNICATION_MAONITOR_H__

#include <pthread.h>

#include <string>
#include <atomic>

#include "alarm.h"

/// @class CommunicationMonitor
///
/// Provides a simple mechanism to track communication state for an entity,
/// and manage alarms for error conditions. Intended use case:
///
///   - whenever an entity successfully communicates with a peer, the
///     inform_success() method should be called
///
///   - whenever an entity fails to communicate with a peer, the 
///     inform_failure() method should be called
///
/// If the monitor detects that all comms are failing over a set_confirm_sec
/// interval, set_alarm_id is generated. Once alarmed the monitor will check
/// for a successful communication at a clear_confirm_sec interval. Once one
/// is detected, clear_alarm_id is generated. Note that timing is driven by
/// calls to the inform_* methods. As such the intervals will not be very
/// precise at low call volume.
class CommunicationMonitor
{
public:
  CommunicationMonitor(const std::string& issuer,
                       const std::string& clear_alarm_id,
                       const std::string& set_alarm_id,
                       unsigned int clear_confirm_sec = 30,
                       unsigned int set_confirm_sec = 15);

 ~CommunicationMonitor();

  /// Report a successful communication. If the current time in ms is available
  /// to the caller it should be passed to avoid duplicate work.
  void inform_success(unsigned long now_ms = 0);

  /// Report a failed communication. If the current time in ms is available to
  /// the caller it should be passed to avoid duplicate work.
  void inform_failure(unsigned long now_ms = 0);

private:
  void update_alarm_state(unsigned long now_ms);
  unsigned long current_time_ms();

  AlarmPair _alarms;

  unsigned int _clear_confirm_ms;
  unsigned int _set_confirm_ms;

  std::atomic<int> _sent;
  std::atomic<int> _failed;

  unsigned long _next_check;

  pthread_mutex_t _lock;
};

#endif

