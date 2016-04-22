/**
 * @file communication_monitor.h
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2015  Metaswitch Networks Ltd
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

#ifndef COMMUNICATION_MONITOR_H__
#define COMMUNICATION_MONITOR_H__

#include "alarm.h"
#include "base_communication_monitor.h"

/// @class CommunicationMonitor
///
/// Provides a simple mechanism to track communication state for an entity,
/// and manage the associated alarm reporting. 
///
/// If the monitor detects that all comms are failing over a set_confirm_sec
/// interval, an alarm active state is requested. Once alarmed the monitor
/// will check for a successful comm. at a clear_confirm_sec interval. Once
/// one is detected, an alarm clear state is requested. Note that timing is
/// driven by calls to the inform_* methods. As such the intervals will not
/// be very precise at low call volume.
///
/// The communication monitor takes ownership of the alarm it's given.
class CommunicationMonitor : public BaseCommunicationMonitor
{
public:
  CommunicationMonitor(Alarm* alarm,
                       std::string sender,
                       std::string receiver,
                       unsigned int clear_confirm_sec = 30,
                       unsigned int set_confirm_sec = 15);

  virtual ~CommunicationMonitor();

private:
  virtual void track_communication_changes(unsigned long now_ms);
  unsigned long current_time_ms();

  Alarm* _alarm;
  std::string _sender;
  std::string _receiver;
  unsigned int _clear_confirm_ms;
  unsigned int _set_confirm_ms;
  unsigned long _next_check;
  int _previous_state;
  // Setup the possible error states
  enum { NO_ERRORS, SOME_ERRORS, ONLY_ERRORS };
};

#endif
