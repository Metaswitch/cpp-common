/**
 * @file communicationmonitor.cpp
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
  _previous_state(0)
{
  _next_check = current_time_ms() + _set_confirm_ms;
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

      int _new_state = 0;
      // Determine the new error state based on the results.
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

      // Check if we need to raise any logs/alarms. We do so if:
      // - We are currently in the NO_ERRORS or SOME_ERRORS states, and
      //   we have seen a change in state in the last 'set_confirm' ms.
      // - We are currently in the ONLY_ERRORS state, and we have
      //   seen a change of state in the last 'clear_confirm' ms.
      switch (_previous_state)
      {
        case NO_ERRORS:
          switch (_new_state)
          {
            case NO_ERRORS: // No change in state. Ensure alarm is cleared.
              _alarm->clear();
              break;

            case SOME_ERRORS:
              CL_CM_CONNECTION_PARTIAL_ERROR.log(_sender.c_str(),
                                                 _receiver.c_str());
              _alarm->clear();
              break;

            case ONLY_ERRORS:
              CL_CM_CONNECTION_ERRORED.log(_sender.c_str(),
                                           _receiver.c_str());
              _alarm->set();
              break;
          }
          break;
        case SOME_ERRORS:
          switch (_new_state)
          {
            case NO_ERRORS:
              CL_CM_CONNECTION_CLEARED.log(_sender.c_str(),
                                           _receiver.c_str());
              _alarm->clear();
              break;

            case SOME_ERRORS: // No change in state. Ensure alarm is cleared.
              _alarm->clear();
              break;

            case ONLY_ERRORS:
              CL_CM_CONNECTION_ERRORED.log(_sender.c_str(),
                                           _receiver.c_str());
              _alarm->set();
              break;
          }
          break;
        case ONLY_ERRORS:
          switch (_new_state)
          {
            case NO_ERRORS:
              CL_CM_CONNECTION_CLEARED.log(_sender.c_str(),
                                           _receiver.c_str());
              _alarm->clear();
              break;

            case SOME_ERRORS:
              CL_CM_CONNECTION_PARTIAL_ERROR.log(_sender.c_str(),
                                                 _receiver.c_str());
              _alarm->clear();
              break;

            case ONLY_ERRORS: // No change in state. Ensure alarm is raised.
              _alarm->set();
              break;
          }
          break;
      }

      // Set the previous state to the new state, as operation is finished.
      _previous_state = _new_state;

      // Set the next check interval.
      _next_check = (_new_state == ONLY_ERRORS) ? now_ms + _clear_confirm_ms :
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
