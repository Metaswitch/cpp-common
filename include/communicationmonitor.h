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

class CMAlarmAdaptor : public AlarmAdaptor
{
public:
  CMAlarmAdaptor(Alarm* alarm,
                 AlarmDef::Severity on_some_errors,
                 AlarmDef::Severity on_all_errors):
    AlarmAdaptor(alarm),
    _on_some_errors(on_some_errors),
    _on_all_errors(on_all_errors)
  {}

  virtual ~CMAlarmAdaptor() {}

  void only_errors()
  {
    _alarm->set(_on_all_errors);
  }

  void some_errors()
  {
    _alarm->set(_on_some_errors);
  }

  void no_errors()
  {
    _alarm->clear();
  }

private:
  AlarmDef::Severity _on_some_errors;
  AlarmDef::Severity _on_all_errors;
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
