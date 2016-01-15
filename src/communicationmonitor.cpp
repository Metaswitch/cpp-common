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
#include "cpp_common_pd_definitions.h"

CommunicationMonitor::CommunicationMonitor(Alarm* alarm,
                                           std::string sender,
                                           std::string receiver,
                                           unsigned int clear_confirm_sec,
                                           unsigned int set_confirm_sec) :
  BaseCommunicationMonitor(),
  _alarm(alarm),
  _sender(sender),
  _receiver(receiver),
  _clear_confirm_ms(clear_confirm_sec * 1000),
  _set_confirm_ms(set_confirm_sec * 1000),
  _previous_state(0),
  _new_state(0)
{
  _next_check = current_time_ms() + _set_confirm_ms;
  // Setup the possible error states
  enum { NO_ERRORS, SOME_ERRORS, ONLY_ERRORS };
}

CommunicationMonitor::~CommunicationMonitor()
{
  delete _alarm;
}

void CommunicationMonitor::track_communication_changes(unsigned long now_ms)
{
  now_ms = now_ms ? now_ms : current_time_ms();

  if (now_ms > _next_check)
  {
    // Current time has passed our monitor interval time, so take the lock
    // and see if we are the lucky thread that gets to check for an alarm
    // condition.
    pthread_mutex_lock(&_lock);

    // If current time is still past the monitor interval time we are the
    // the lucky one, otherwise somebody beat us to the punch (so just drop
    // the lock and return).
    if (now_ms > _next_check)
    {
      // Grab the current counts and reset them to zero in a lockless manner.
      unsigned int succeeded = _succeeded.fetch_and(0);
      unsigned int failed = _failed.fetch_and(0);
      TRC_DEBUG("Checking communication changes - successful attempts %d, failures %d",
                succeeded, failed);

      // Determine the current error state based on the results.
      // If no successes of failures, new state remains unchanged.
      // States:
      // NO_ERRORS: At least one success and no failures
      // SOME_ERRORS: At least one success and at least one failure
      // ONLY_ERRORS: No successes and at least one failure
      if ((succeeded != 0) && (failed == 0))
      {
        _new_state = NO_ERRORS;
      }
      else if ((succeeded != 0) && (failed != 0))
      {
        _new_state = SOME_ERRORS;
      }
      else if ((succeeded == 0) && (failed != 0))
      {
        _new_state = ONLY_ERRORS;
      }

      // Check if we need to raise any logs/alarms. We do so if the new
      // error state is different from the the previous error state.
      //
      // Switch organised by previous state, then new state.
      switch (_previous_state)
      {
        case NO_ERRORS:
          switch (_new_state)
          {
            case NO_ERRORS:
              // No change in state. Do nothing.
              break;

            case SOME_ERRORS:
              // Raise the partial error log
              CL_CM_CONNECTION__PARTIAL_ERROR.log(_sender.c_str(),
                                                  _receiver.c_str());
              break;

            case ONLY_ERRORS:
              // Raise the full error log, and set alarm
              CL_CM_CONNECTION_ERRORED.log(_sender.c_str(),
                                           _receiver.c_str());
              TRC_STATUS("Setting alarm %d", _alarm->index());
              _alarm->set();
              break;
          }
        case SOME_ERRORS:
          switch (_new_state)
          {
            case NO_ERRORS:
              // Clear the partial error log
              CL_CM_CONNECTION_CLEARED.log(_sender.c_str(),
                                           _receiver.c_str());
              break;

            case SOME_ERRORS:
              // No change in state. Do nothing.
              break;

            case ONLY_ERRORS:
              // Raise the full error log, and set the alarm.
              CL_CM_CONNECTION_ERRORED.log(_sender.c_str(),
                                           _receiver.c_str());
              TRC_STATUS("Setting alarm %d", _alarm->index());
              _alarm->set();
              break;
          }
        case ONLY_ERRORS:
          switch (_new_state)
          {
            case NO_ERRORS:
              // Clear the full error log, clear alarm.
              CL_CM_CONNECTION_CLEARED.log(_sender.c_str(),
                                           _receiver.c_str());
              TRC_STATUS("Clearing alarm %d", _alarm->index());
              _alarm->clear();
              break;

            case SOME_ERRORS:
              // Clear the alarm, raise the partial error log.
              CL_CM_CONNECTION_CLEARED.log(_sender.c_str(),
                                           _receiver.c_str());
              TRC_STATUS("Clearing alarm %d", _alarm->index());
              _alarm->clear();
              break;

            case ONLY_ERRORS:
              // No change in state. Do nothing.
              break;
          }
      }

      // Set the previous state to the new state, as operation is finished.
      _previous_state = _new_state;

      // Set the next check interval.
      // Evaluates TRUE if either SOME_ERRORS or ONLY_ERRORS.
      _next_check = _new_state ? now_ms + _clear_confirm_ms :
                                   now_ms + _set_confirm_ms;
    }

    pthread_mutex_unlock(&_lock);
  }
}

unsigned long CommunicationMonitor::current_time_ms()
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);

  return ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
}
