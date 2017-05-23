/**
 * @file pdlog.h Enhanced Node Troubleshooting PDLog classes
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */


#ifndef PDLOG_H__
#define PDLOG_H__

//
// PDLog Classes contain the Description, Cause, Effect, and Action for a log
//

#include <string>
#include <syslog.h>

// Defines common definitions for PDLog (Problem Definition Log) classes

// A PDLogBase defines the base class containing:
//   Identity - Identifies the log id to be used in the syslog id field.
//   Severity - One of Emergency, Alert, Critical, Error, Warning, Notice,
//              and Info.  Only Error and Notice are used.
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
  // The following must be kept in sync with
  // https://github.com/Metaswitch/python-common/blob/dev/metaswitch/common/pdlogs.py
  enum PDNodeType
  {
    CL_CPP_COMMON_ID = 1000,
    CL_SPROUT_ID = 2000,
    CL_CHRONOS_ID = 3000,
    CL_HOMESTEAD_ID = 4000,
    CL_RALF_ID = 5000,
    CL_SCRIPT_ID = 6000,
    CL_ASTAIRE_ID = 7000,
    CL_CLUSTER_MGR_ID = 8000,
    CL_CONFIG_MGR_ID = 9000,
    // The range 10000-11999 is reserved
    CL_PYTHON_COMMON_ID = 12000,
    CL_CREST_ID = 13000,
    CL_QUEUE_MGR_ID = 14000
    // The range 15000-15999 is reserved
    // The range 16000-16999 is reserved
    // The range 17000-17999 is reserved
    // The range 18000-18999 is reserved
    // The range 19000-19999 is reserved
  };

  PDLogBase(int log_id,
            int severity,
            const std::string& desc,    // Description of the condition
            const std::string& cause,   // The cause of the condition
            const std::string& effect,  // The effect the condiiton has on the system
            const std::string& action)  // A list of actions to be taken for the condition
    : _log_id(log_id), _severity(severity)
  {
    _msg = ("Description: " + desc + " @@Cause: " + cause + " @@Effect: " + effect + " @@Action: " + action) ;
  }

  // Writes the description. cause, effect, and actions to syslog
  virtual void dcealog(const char* buf) const
  {
    syslog(_severity, "%d - %s", _log_id, buf);
  }
protected:
  // Unique identity for a PDLog, e.g. CL_CPP_COMMON + 1
  int         _log_id;

  // Log severity, usually LOG_ERR or LOG_NOTICE
  int         _severity;

  std::string _msg;
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

  virtual ~PDLog() {}

  virtual void log() const
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

  virtual ~PDLog1() {}

  virtual void log(T1 v1) const
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

  virtual ~PDLog2() {}

  virtual void log(T1 v1, T2 v2) const
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

  virtual ~PDLog3() {}

  virtual void log(T1 v1, T2 v2, T3 v3) const
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

  virtual ~PDLog4() {}

  virtual void log(T1 v1, T2 v2, T3 v3, T4 v4) const
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
