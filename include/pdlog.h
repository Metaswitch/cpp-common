/**
 * @file pdlog.h Enhanced Node Troubleshooting PDLog classes 
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


#ifndef PDLOG_H__
#define PDLOG_H__

//
// PDLog Classes contain the Description, Cause, Effect, and Action for a log
//

#include <string>

extern "C" {
  // syslog_facade prevents name collisions between the existing
  // Clearwater logging (LOG_) and the definitions in syslog.h
#include "syslog_facade.h"
}

// Defines common definitions for PDLog (Problem Definition Log) classes

// A PDLogBase defines the base class containing:
//   Identity - Identifies the log id to be used in the syslog id field.
//   Severity - One of Emergency, Alert, Critical, Error, Warning, Notice, 
//              and Info.  Directly corresponds to the syslog severity types.
//              Only Error and Notice are used.  See syslog_facade.h for 
//              definitions.
//   Message - Formatted description of the condition.
//   Cause - The cause of the condition.
//   Effect - The effect the condition.
//   Action - A list of one or more actions to take to resolve the condition 
//           if it is an error.
// The elements of the class are used to format a syslog call.
// The call to output to syslog is in the method,  dcealog.
// By default syslog limits a total syslog message size to 2048 bytes.  
// Anything above the limit is truncated.  The formatted message, cause, 
// effect, and action(s) are concatenated into the syslog message.  Note, 
// as an arbitrary convention, for more than a signle action, the actions 
// are numbered as (1)., (2)., ...  to make the actions easier to read within 
// the syslog message.  syslog removes extra whitespace and
// carriage-returns/linefeeds before inserting the complete string into a 
// message.  Note also, the action(s) are a list of strings with all but 
// the last string having a space character at the end.  The space makes the 
// actions more readable.  Most of the derived classes are templates.  
// The paremeterized types being values that are output as a formatted string 
// in the Message field.
class PDLogBase
{
public:
  static const int MAX_FORMAT_LINE = 1024;
  // Identifies the application type reporting the log
  enum PDNodeType
  {
    CL_CPP_COMMON_ID = 1000,
    CL_SPROUT_ID = 2000,
    CL_CHRONOS_ID = 3000,
    CL_HOMESTEAD_ID = 4000,
    CL_RALF_ID = 5000,
    CL_SCRIPT_ID = 6000
  };

  PDLogBase(int log_id, int severity, const std::string& msg, 
            const std::string& cause,
            const std::string& effect, const std::string& action) 
    : _log_id(log_id), _severity(severity), _msg(msg),
    _cause(cause), _effect(effect), _action(action) {};

  // Writes the description. cause, effect, and actions to syslog
  virtual void dcealog(const char* buf) const
  {
    syslog(_severity, "%d - Description: %s @@Cause: %s @@Effect: "
	   "%s @@Action: %s", 
	   _log_id, buf, _cause.c_str(), _effect.c_str(), _action.c_str());
  }
protected:
  // Unique identity for a PDLog, e.g. CL_CPP_COMMON + 1
  int         _log_id;

  // Log severity, usually PDLOG_ERR or PDLOG_NOTICE
  int         _severity;

  // Description of the condition
  std::string _msg;

  // The cause of the condition
  std::string _cause;

  // The effect the condiiton has on the system
  std::string _effect;

  // A list of actions to be taken for the condition
  std::string _action;
};

// PDLog - For logs with no log() arguments
class PDLog : public PDLogBase
{
public:
  PDLog(int log_id,
        int severity,
        const std::string& msg,
        const std::string& cause,
        const std::string& effect,
        const std::string&  action)
    : PDLogBase(log_id, severity, msg, cause, effect, action)
  {
  };

  void log() const
  {
    // The format for the snprintf is defined by buf
    char buf[MAX_FORMAT_LINE];
    // The pragmas are used to avoid compiler warnings
    // Normally this would be a security issue but the
    // template type insures the log call agrees with the format
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    snprintf(buf, MAX_FORMAT_LINE - 2, (const char*)_msg.c_str());
#pragma GCC diagnostic pop
    dcealog(buf);
  };

};

// PDLog with one log method argument -- The log argument type is T1
template<class T1> class PDLog1 : public PDLogBase
{
public:
  PDLog1(int log_id,
         int severity,
         const std::string& msg,
         const std::string& cause,
         const std::string& effect,
         const std::string& action)
    : PDLogBase(log_id, severity, msg, cause, effect, action)
  {
  };

  void log(T1 v1) const
  {
    // The format for the snprintf is defined by buf
    char buf[MAX_FORMAT_LINE];
    // The pragmas are used to avoid compiler warnings
    // Normally this would be a security issue but the
    // template type insures the log call agrees with the format
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    snprintf(buf, MAX_FORMAT_LINE - 2, (const char*)_msg.c_str(), v1);
#pragma GCC diagnostic pop
    dcealog(buf);
  };

};

// PDLog with two log arguments -- The log argument types are T1 and T2
template<class T1, class T2> class PDLog2 : public PDLogBase
{
public:
  PDLog2(int log_id,
         int severity,
         const std::string& msg,
         const std::string& cause,
         const std::string& effect,
         const std::string& action)
    : PDLogBase(log_id, severity, msg, cause, effect, action)
  {
  };

  void log(T1 v1, T2 v2) const
  {
    char buf[MAX_FORMAT_LINE];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    snprintf(buf, MAX_FORMAT_LINE - 2, (const char*)_msg.c_str(), v1, v2);
#pragma GCC diagnostic pop
    dcealog(buf);
  };

};

// PDLog with three log arguments -- The log argument types are T1, T2, and T3
template<class T1, class T2, class T3> class PDLog3 : public PDLogBase
{
public:
  PDLog3(int log_id,
         int severity,
         const std::string& msg,
         const std::string& cause,
         const std::string& effect,
         const std::string& action)
    : PDLogBase(log_id, severity, msg, cause, effect, action)
  {
  };

  void log(T1 v1, T2 v2, T3 v3) const
  {
    char buf[MAX_FORMAT_LINE];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    snprintf(buf, MAX_FORMAT_LINE - 2, (const char*)_msg.c_str(), v1, v2, v3);
#pragma GCC diagnostic pop
    dcealog(buf);
  };

};


// PDLOg with four log arguments - The log argument types are T1, T2, T3, and T4
template<class T1, class T2, class T3, class T4> class PDLog4 : public PDLogBase
{
public:
  PDLog4(int log_id,
         int severity,
         const std::string& msg,
         const std::string& cause,
         const std::string& effect,
         const std::string& action)
    : PDLogBase(log_id, severity, msg, cause, effect, action)
  {
  };

  void log(T1 v1, T2 v2, T3 v3, T4 v4) const
  {
    char buf[MAX_FORMAT_LINE];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    snprintf(buf, MAX_FORMAT_LINE - 2, (const char*)_msg.c_str(), 
	     v1, v2, v3, v4);
#pragma GCC diagnostic pop
    dcealog(buf);
  };

};

#endif
