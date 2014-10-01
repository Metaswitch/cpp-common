/**
 * @file craft_ent_definitions.h Craft ENT Log PDLog classes, Description, Cause, Effect. Action Definitions
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


#ifndef CRAFT_ENT_DEFINITIONS_H__
#define CRAFT_ENT_DEFINITIONS_H__

//
// Craft Logging(syslog) PDLog Classes, Description, Cause, Effect, and Action
//

#include <string>

extern "C" {
#include "syslog_facade.h"
}

extern const char* signalnames[];
///
/// Defines common definitions for PDLog classes
class PDLogBase
{
public:
  enum
  {
    CL_CPP_COMMON_ID = 1000,
    CL_SPROUT_ID = 2000,
    CL_CHRONOS_ID = 3000,
    CL_HOMESTEAD_ID = 4000,
    CL_RALF_ID = 5000
  };
  PDLogBase(int log_id, int severity, const std::string& msg, const std::string& cause, 
	    const std::string& effect, const std::string& action) : _log_id(log_id), _severity(severity), _msg(msg), 
    _cause(cause), _effect(effect), _action(action) {};

  // Writes the description. cause, effect, and actions to syslog
  virtual void dcealog(const char* buf) const
  {
    syslog(_severity, "%d - Description: %s Cause: %s Effect: %s Action: %s", _log_id, buf, _cause.c_str(), _effect.c_str(), _action.c_str());
  }
protected:
  ///
  /// Unique identity for a PDLog, e.g. CL_CPP_COMMON + 1
  int         _log_id;

  ///
  /// Log severity, usually PDLOG_ERR or PDLOG_NOTICE
  int         _severity;

  /// Description of the condition
  std::string _msg;

  /// The cause of the condition
  std::string _cause;

  /// The effect the condiiton has on the system
  std::string _effect;

/// A list of actions to be taken for the condition
  std::string _action;
};

///
/// PDLog - For logs with no log() arguments
class PDLog : public PDLogBase
{
public:
  PDLog(int log_id, int severity, const std::string& msg, const std::string& cause,
        const std::string& effect,
        const std::string&  action) : PDLogBase(log_id, severity, msg, cause, effect, action)
  {
  };
  void log() const
  {
    char buf[256];
    // The format for the sprintf is defined by buf
    // The pragmas are used to avoid compiler warnings
    // Normally this would be a security issue but the 
    // template type insures the log call agrrems with the format
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    sprintf(buf, (const char*)_msg.c_str());
#pragma GCC diagnostic pop
    dcealog(buf);
  };

protected:
};

///
/// PDLOg with one log method argument -- The log argument type is T1
template<class T1> class PDLog1 : public PDLogBase
{
public:
  PDLog1(int log_id, int severity, const std::string& msg, const std::string& cause,
         const std::string& effect,
         const std::string& action) : PDLogBase(log_id, severity, msg, cause, effect, action)
  {
  };
  void log(T1 v1) const
  {
    char buf[256];
    // The format for the sprintf is defined by buf
    // The pragmas are used to avoid compiler warnings
    // Normally this would be a security issue but the 
    // template type insures the log call agrrems with the format
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    sprintf(buf, (const char*)_msg.c_str(), v1);
#pragma GCC diagnostic pop
    dcealog(buf);
  };

protected:
};

///
/// PDLOg with two log arguments -- The log argument types are T1 and T2
template<class T1, class T2> class PDLog2 : public PDLogBase
{
public:
  PDLog2(int log_id, int severity, const std::string& msg, const std::string& cause,
         const std::string& effect,
         const std::string& action, ...) : PDLogBase(log_id, severity, msg, cause, effect, action)
  {
  };
  void log(T1 v1, T2 v2) const
  {
    char buf[256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    sprintf(buf, (const char*)_msg.c_str(), v1, v2);
#pragma GCC diagnostic pop
    dcealog(buf);
  };

protected:
};

///
/// PDLOg with three log arguments -- The log argument types are T1, T2, and T3
template<class T1, class T2, class T3> class PDLog3 : public PDLogBase
{
public:
  PDLog3(int log_id, int severity, const std::string& msg, const std::string& cause,
         const std::string& effect,
         const std::string& action, ...) : PDLogBase(log_id, severity, msg, cause, effect, action)
  {
  };
  void log(T1 v1, T2 v2, T3 v3) const
  {
    char buf[256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    sprintf(buf, (const char*)_msg.c_str(), v1, v2, v3);
#pragma GCC diagnostic pop
    dcealog(buf);
  };

protected:
};


///
/// PDLOg with four log arguments -- The log argument types are T1, T2, T3, and T4
///
template<class T1, class T2, class T3, class T4> class PDLog4 : public PDLogBase
{
public:
  PDLog4(int log_id, int severity, const std::string& msg, const std::string& cause,
         const std::string& effect,
         const std::string& action) : PDLogBase(log_id, severity, msg, cause, effect, action)
  {
  };
  void log(T1 v1, T2 v2, T3 v3, T4 v4) const
  {
    char buf[256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    sprintf(buf, (const char*)_msg.c_str(), v1, v2, v3, v4);
#pragma GCC diagnostic pop
    dcealog(buf);
  };

protected:
};


/// CPP_COMMON syslog identities
/**********************************************************
/ log_id
/ severity
/ Description: (formatted)
/ Cause:
/ Effect:
/ Action:
**********************************************************/
static const PDLog CL_DIAMETER_START
(
  PDLogBase::CL_CPP_COMMON_ID + 1,
  PDLOG_NOTICE,
  "Diameter stack is starting",
  "Diameter stack is beginning initialization",
  "Normal",
  "None"
);
static const PDLog CL_DIAMETER_INIT_CMPL
(
  PDLogBase::CL_CPP_COMMON_ID + 2,
  PDLOG_NOTICE,
  "Diameter stack initialization completed",
  "Diameter stack has completed initialization",
  "Normal",
  "None"
);
static const PDLog4<const char*, int, const char*, const char*> CL_DIAMETER_ROUTE_ERR
(
  PDLogBase::CL_CPP_COMMON_ID + 3,
  PDLOG_ERR,
  "Diameter routing error: %s for message with Command-Code %d, Destination-Host %s and Destination-Realm %s",
  "No route was found for a Diameter message",
  "The Diameter message with the specified command code could not be routed to the destination host with the destination realm",
  "(1). Check the installation guide for Diameter host configuration. "
  "(2). Check to see that there is a route to the destination host. "
  "Check for IP connectivity between the homestead host and the hss host using ping. "
  "Wireshark the interface on homestead and the hss"
);
static const PDLog1<const char*> CL_DIAMETER_CONN_ERR
(
  PDLogBase::CL_CPP_COMMON_ID + 4,
  PDLOG_ERR,
  "Failed to make a Diameter connection to host %s",
  "A Diameter connection attempt failed to the specified host",
  "This impacts the ability to register, subscribe, or make a call",
  "(1). Check the installation guide for Diameter host configuration. "
  "(2). Check to see that there is a route to the destination host. "
  "Check for IP connectivity between the homestead host and the hss host using ping. "
  "Wireshark the interface on homestead and the hss"
);
static const PDLog4<const char*, const char*, const char*, int> CL_HTTP_COMM_ERR
(
  PDLogBase::CL_CPP_COMMON_ID + 5,
  PDLOG_ERR,
  "%s failed to communicate with http server %s with curl error %s code %d",
  "An HTTP connection attempt failed to the specified server with the specified error code",
  "This condition impacts the ability to register, subscribe, or make a call.",
  "(1). Check to see if the specified host has failed. "
  "(2). Check to see if there is TCP connectivity to the host by using ping and/or Wireshark."
);

#endif
