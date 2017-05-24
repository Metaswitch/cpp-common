/**
 * @file communication_monitor.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef COMMUNICATION_MONITOR_H__
#define COMMUNICATION_MONITOR_H__

#include "alarm.h"
#include "base_communication_monitor.h"

/// @class CMAlarmAdaptor
///
/// The communication monitor tracks three types of communication events:
///  - all communication attempts in the last interval were successful
///  - all communication attempts in the last interval failed
///  - there were successful and failed communication attempts in the last
///    interval
///
/// Different users of the communication monitor will want to raise different
/// alarms at different severities for these three events - this class provides
/// a translation between the communication monitor event, and the user raising
/// the correct alarm.
class CMAlarmAdaptor
{
public:
  CMAlarmAdaptor(Alarm* alarm,
                 AlarmDef::Severity severity_for_some_errors,
                 AlarmDef::Severity severity_for_only_errors):
    _alarm(alarm),
    _severity_for_some_errors(severity_for_some_errors),
    _severity_for_only_errors(severity_for_only_errors)
  {}

  virtual ~CMAlarmAdaptor()
  {
    delete _alarm;
  }

  // Sets an alarm if there have been no successful communication attempts in
  // the last interval (and at least one attempt was made).
  void only_comm_errors()
  {
    _alarm->set(_severity_for_only_errors);
  }

  // Sets an alarm if there has been a mix of successful and failed
  // communication attempts in the last interval.
  void some_comm_errors()
  {
    _alarm->set(_severity_for_some_errors);
  }

  // Clears the alarm if there have been no failed communication attempts in
  // the last interval.
  void no_comm_errors()
  {
    _alarm->clear();
  }

private:
  Alarm* _alarm;
  AlarmDef::Severity _severity_for_some_errors;
  AlarmDef::Severity _severity_for_only_errors;
};

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
  CommunicationMonitor(CMAlarmAdaptor* alarm_adaptor,
                       std::string sender,
                       std::string receiver,
                       unsigned int clear_confirm_sec = 30,
                       unsigned int set_confirm_sec = 15);

  virtual ~CommunicationMonitor();

private:
  virtual void track_communication_changes(unsigned long now_ms);
  unsigned long current_time_ms();

  CMAlarmAdaptor* _alarm_adaptor;
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
