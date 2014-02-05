/**
 * @file log.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
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


#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "log.h"

const char* log_level[] = {"Error", "Warning", "Status", "Info", "Verbose", "Debug"};

#define MAX_LOGLINE 8192

namespace Log
{
  static Logger *logger = new Logger();
  static int loggingLevel = 4;
}

void Log::setLoggingLevel(int level)
{
  if (level > DEBUG_LEVEL)
  {
    level = DEBUG_LEVEL;
  }
  else if (level < ERROR_LEVEL)
  {
    level = ERROR_LEVEL;
  }
  Log::loggingLevel = level;
}

void Log::setLogger(Logger *log)
{
  if (log != NULL)
  {
    // This is not a logger deregistering itself, so need to delete the old
    // logger to avoid leaking it.
    delete Log::logger;
  }
  Log::logger = log;
  if (Log::logger != NULL)
  {
    Log::logger->set_flags(Logger::FLUSH_ON_WRITE|Logger::ADD_TIMESTAMPS);
  }
}

void Log::write(int level, const char *module, int line_number, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  _write(level, module, line_number, fmt, args);
  va_end(args);
}

void Log::_write(int level, const char *module, int line_number, const char *fmt, va_list args)
{
  if (!Log::logger)
  {
    return;
  }

  if (level > Log::loggingLevel)
  {
    return;
  }

  char logline[MAX_LOGLINE];

  int written = 0;

  const char* mod = strrchr(module, '/');
  module = (mod != NULL) ? mod + 1 : module;

  if (line_number)
  {
    written = snprintf(logline, MAX_LOGLINE - 2, "%s %s:%d: ", log_level[level], module, line_number);
  }
  else
  {
    written = snprintf(logline, MAX_LOGLINE - 2, "%s %s: ", log_level[level], module);
  }

  written += vsnprintf(logline + written, MAX_LOGLINE - written - 2, fmt, args);

  // Add a new line and null termination.
  logline[written] = '\n';
  logline[written+1] = '\0';

  Log::logger->write(logline);
}

// LCOV_EXCL_START Only used in exceptional signal handlers - not hit in UT

void Log::backtrace(const char *fmt, ...)
{
  if (!Log::logger)
  {
    return;
  }

  va_list args;
  char logline[MAX_LOGLINE];
  va_start(args, fmt);
  int written = vsnprintf(logline, MAX_LOGLINE, fmt, args);
  va_end(args);

  // Add a new line and null termination.
  logline[written] = '\n';
  logline[written+1] = '\0';

  Log::logger->backtrace(logline);
}

// LCOV_EXCL_STOP
