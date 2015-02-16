/**
 * @file fakelogger.hpp Header file for fake logger (for testing).
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

///
///----------------------------------------------------------------------------

#ifndef FAKELOGGER_H_
#define FAKELOGGER_H_

#include <string>
#include "log.h"
#include "logger.h"

const int DEFAULT_LOGGING_LEVEL = 4;

// Base class of PrintingTestLogger and CapturingTestLogger. Provides
// the "log to stdout" functionality that's common to both, but
// doesn't set/unset itself as the logger on construction/destruction.

class BaseTestLogger : public Logger
{
public:
BaseTestLogger():
  _last_logger(NULL),
  _last_logging_level(DEFAULT_LOGGING_LEVEL)
 {};

  virtual ~BaseTestLogger()
  {
  }


  virtual void write(const char* data);
  void flush();

  bool isPrinting();
  void setPrinting(bool printing);

  void setLoggingLevel(int level);

  void setupFromEnvironment();
  void take_over();
  void relinquish_control();

protected:
  bool _noisy;
  Logger* _last_logger;
  int _last_logging_level;
};

/// Logger that prints logged items to stdout. This is just the
/// function inherited from BaseTestLogger, plus a
/// constructor/destructor that set/unset this as the global logger.
///
/// PrintingTestLogger::DEFAULT should be the only instance needed.
class PrintingTestLogger : public BaseTestLogger
{
public:
  PrintingTestLogger();

  virtual ~PrintingTestLogger();

  static PrintingTestLogger DEFAULT;
};


// Besides printing logs to stdout, captures them to an
// internal buffer and provides a contains() method for checking what
// was logged. Be wary of using this as it leads to test fragility.

// On construction, sets the log level to 99 to avoid false positives.
// Its scope should therefore be kept as small as possible to avoid
// this.

// On destruction, reinstates PrintingTestLogger::DEFAULT as the
// logger. This includes setting the logging level back from 99 to the
// value based on the NOISY environment variable.
class CapturingTestLogger: public BaseTestLogger
{
public:

  CapturingTestLogger();
  CapturingTestLogger(int level);
  ~CapturingTestLogger();
  
  void write(const char* line);
  bool contains(const char* fragment);

private:
  std::string _logged;
  pthread_mutex_t _logger_lock;
};

#endif
