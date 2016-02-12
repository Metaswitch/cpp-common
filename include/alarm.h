/**
 * @file alarm.h
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

#ifndef ALARM_H__
#define ALARM_H__

#include <pthread.h>

#include <string>
#include <vector>
#include <atomic>

#include "alarmdefinition.h"
#include "eventq.h"

/// @class AlarmState
///
/// Provides the basic interface for updating an SNMP alarm's state.

class AlarmState
{
public:
  AlarmState(const std::string& issuer,
             const int index,
             AlarmDef::Severity severity);

  /// Queue request to update the alarm identified by index, to the
  /// state associated with severity, for the specified issuer.
  void issue();

  /// Queue request to clear all active alarms initiated by issuer.
  /// This will result in a state change to CLEARED for all alarms
  /// whose state was set to non-CLEARED by the issuer.
  static void clear_all(const std::string& issuer);

  std::string& get_issuer() {return _issuer;}
  std::string& get_identifier() {return _identifier;}

private:
  std::string _issuer;
  std::string _identifier;
};

/// @class BaseAlarm
/// 
/// Superclass for alarms that allows us to split different types of
/// alarms into subclasses. Those alarms which only have one possible raised
/// state will be constructed by subclass Alarm. Those alarms which have two or
/// more possible raised states will be constructed by subclass
/// MultiStateAlarm. 

class BaseAlarm
{
public:
  BaseAlarm(const std::string& issuer,
            const int index):
    _index(index),
    _clear_state(issuer, index, AlarmDef::CLEARED),
    _alarmed(false)
  {
  }
  
  /// Queues a request to generate an alarm state change corresponding to the
  /// CLEARED severity.
  virtual void clear();

protected:
  const int _index;
  AlarmState _clear_state;
  
  /// Keeps track of whether the alarm is raised
  std::atomic<bool> _alarmed;
};

/// @class Alarm
///
/// Encapsulates an alarm's only active state with its associated alarm clear state.
/// Used to manage the reporting of a fault condition, and subsequent clear
/// of said condition.

class Alarm: public BaseAlarm
{
public:
  Alarm(const std::string& issuer,
        const int index,
        AlarmDef::Severity severity);

  virtual ~Alarm() {}

  /// Queues a request to generate an alarm state change corresponding to the
  /// non-CLEARED severity if a state change for the CLEARED severity was
  /// previously requestd via clear().
  virtual void set();

  /// Returns the index of this alarm.
  virtual int index() const {return _index;}
  
  /// Indicates whether the alarm state currently maintained by this object
  /// corresponds to the non-CLEARED severity.
  virtual bool alarmed() {return _alarmed.load();}

private:
  AlarmState _set_state;
};

/// @class MultiStateAlarm
///
/// Encapsulates an alarm's two or more active states with its associated clear
/// state. Used to manage the reporting of a fault condition, and subsequent
/// clear of said condition.

class MultiStateAlarm: public BaseAlarm
{
public:
  MultiStateAlarm(const std::string& issuer,
                  const int index);
  
  /// Indicates whether the alarm state currently maintained by this object
  /// corresponds to the non-CLEARED severity.
  virtual bool alarmed() {return _alarmed.load();}
  
protected:
  /// These raise the alarm with the specified severity.
  virtual void multi_set_indeterminate();
  virtual void multi_set_warning();
  virtual void multi_set_minor();
  virtual void multi_set_major();
  virtual void multi_set_critical();

private:
  AlarmState _indeterminate_state;
  AlarmState _warning_state;
  AlarmState _minor_state;
  AlarmState _major_state;
  AlarmState _critical_state;
};

/// @class CedarLicenseErrorAlarm
///
/// Represents a Clearwater alarm with multiple raised states. This class gives
/// the MultiStateAlarm object visibility of just those functions corresponding
/// to states of the alarm that can be raised.

class CedarLicenseErrorAlarm: public MultiStateAlarm
{
public:
  CedarLicenseErrorAlarm(const std::string& issuer,
                             const int index):
    MultiStateAlarm(issuer,
                    index){}
  virtual void set_major() {MultiStateAlarm::multi_set_major();}
  virtual void set_critical() {MultiStateAlarm::multi_set_critical();}
};

/// @class CedarLicenseErrorAlarm
///
/// Represents a Clearwater alarm with multiple raised states. This class gives
/// the MultiStateAlarm object visibility of just those functions corresponding
/// to states of the alarm that can be raised.

//class CedarFTPDLicenseErrorAlarm: public MultiStateAlarm
//{
//public:
  //CedarFTPDLicenseErrorAlarm(const std::string& issuer,
    //                         const int index):
    //MultiStateAlarm(issuer,
    //                index){}
  //virtual void set_major() {MultiStateAlarm::multi_set_major();}
  //virtual void set_critical() {MultiStateAlarm::multi_set_critical();}
//};

/// @class AlarmReqAgent
///
/// Singleton which provides an agent thead to accept queued alarm requests from
/// clients and forward them via ZMQ to snmpd (which will actually generate the
/// inform message(s)).

class AlarmReqAgent
{
public:
  enum
  {
    MAX_Q_DEPTH = 100
  };

  /// Initialize ZMQ context and start agent thread.
  bool start();

  /// Gracefully stop the agent thread and remove ZMQ context.
  void stop();

  /// Queue an alarm request to be forwarded to snmpd.
  void alarm_request(std::vector<std::string> req);

  static AlarmReqAgent& get_instance() {return _instance;}

private:
  enum
  {
    ZMQ_PORT = 6664,
    MAX_REPLY_LEN = 16
  };

  static void* agent_thread(void* alarm_req_agent);

  AlarmReqAgent();

  bool zmq_init_ctx();
  bool zmq_init_sck();

  void zmq_clean_ctx();
  void zmq_clean_sck();

  void agent();

  pthread_t _thread;

  pthread_mutex_t _start_mutex;
  pthread_cond_t  _start_cond;

  void* _ctx;
  void* _sck;

  eventq<std::vector<std::string> >* _req_q;

  static AlarmReqAgent _instance;
};

#endif
