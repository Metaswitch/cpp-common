/**
 * @file craft_dcea.h Craft Log Description, Cause, Effect. Action Definitions
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


#ifndef CRAFT_DCEA_H__
#define CRAFT_DCEA_H__

//
// Craft Logging(syslog) Log Description, Cause, Effect, and Action
//

extern "C" {
#include "syslog_facade.h"
}

#define CL_CPP_COMMON_ID 1000
#define CL_SPROUT_ID 2000
#define CL_CHRONOS_ID 3000
#define CL_HOMESTEAD_ID 4000
#define CL_RALF_ID 5000


class SysLog {
 public:
 SysLog(int log_id, int severity, const std::string& msg, const std::string& cause, const std::string& effect, const std::string& action) : _log_id(log_id), _severity(severity), _msg(msg), _cause(cause), _effect(effect), _action(action) {};
  void log() {
    char buf[256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    sprintf(buf, (const char*)_msg.c_str());
#pragma GCC diagnostic pop
    syslog(_severity, "%d - %s", _log_id, buf);
  };

 protected:
  int         _log_id;
  int         _severity;
  std::string _msg;
  std::string _cause;
  std::string _effect;
  std::string _action;
};

template<class T1> class SysLog1 {
 public:
 SysLog1(int log_id, int severity, const std::string& msg, const std::string& cause, const std::string& effect, const std::string& action) : _log_id(log_id), _severity(severity), _msg(msg), _cause(cause), _effect(effect), _action(action) {};
  void log(T1 v1) {
    char buf[256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    sprintf(buf, (const char*)_msg.c_str(), v1);
#pragma GCC diagnostic pop
    syslog(_severity, "%d - %s", _log_id, buf);
  };

 protected:
  int         _log_id;
  int         _severity;
  std::string _msg;
  std::string _cause;
  std::string _effect;
  std::string _action;
};

template<class T1, class T2> class SysLog2 {
 public:
 SysLog2(int log_id, int severity, const std::string& msg, const std::string& cause, const std::string& effect, const std::string& action) : _log_id(log_id), _severity(severity), _msg(msg), _cause(cause), _effect(effect), _action(action) {};
  void log(T1 v1, T2 v2) {
    char buf[256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    sprintf(buf, (const char*)_msg.c_str(), v1, v2);
#pragma GCC diagnostic pop
    syslog(_severity, "%d - %s", _log_id, buf);
  };

 protected:
  int         _log_id;
  int         _severity;
  std::string _msg;
  std::string _cause;
  std::string _effect;
  std::string _action;
};

template<class T1, class T2, class T3> class SysLog3 {
 public:
 SysLog3(int log_id, int severity, const std::string& msg, const std::string& cause, const std::string& effect, const std::string& action) : _log_id(log_id), _severity(severity), _msg(msg), _cause(cause), _effect(effect), _action(action) {};
  void log(T1 v1, T2 v2, T3 v3) {
    char buf[256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    sprintf(buf, (const char*)_msg.c_str(), v1, v2, v3);
#pragma GCC diagnostic pop
    syslog(_severity, "%d - %s", _log_id, buf);
  };

 protected:
  int         _log_id;
  int         _severity;
  std::string _msg;
  std::string _cause;
  std::string _effect;
  std::string _action;
};

template<class T1, class T2, class T3, class T4> class SysLog4 {
 public:
 SysLog4(int log_id, int severity, const std::string& msg, const std::string& cause, const std::string& effect, const std::string& action) : _log_id(log_id), _severity(severity), _msg(msg), _cause(cause), _effect(effect), _action(action) {};
  void log(T1 v1, T2 v2, T3 v3, T4 v4) {
    char buf[256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    sprintf(buf, (const char*)_msg.c_str(), v1, v2, v3, v4);
#pragma GCC diagnostic pop
    syslog(_severity, "%d - %s", _log_id, buf);
  };

 protected:
  int         _log_id;
  int         _severity;
  std::string _msg;
  std::string _cause;
  std::string _effect;
  std::string _action;
};




extern SysLog CL_DIAMETER_START;
extern SysLog  CL_DIAMETER_INIT_CMPL;
extern SysLog4<const char*, int, const char*, const char*> CL_DIAMETER_ROUTE_ERR;
extern SysLog1<const char*> CL_DIAMETER_CONN_ERR;
extern SysLog4<const char*, const char*, const char*, int> CL_HTTP_COMM_ERR;


#endif
