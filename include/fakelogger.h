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

/// Logger that prints logged items to stdout.
/// PrintingTestLogger::DEFAULT should be the only instance needed.
class PrintingTestLogger : public Logger
{
public:
  /// Get a logger with the default behaviour.
  PrintingTestLogger();

  virtual ~PrintingTestLogger();

  virtual void write(const char* data);
  void flush();

  bool isPrinting();
  void setPrinting(bool printing);

  void setLoggingLevel(int level);

  void setupFromEnvironment();
  void take_over();

  static PrintingTestLogger DEFAULT;

protected:
  bool _noisy;
  Logger* _last_logger;
};

// Besides the function of PrintingTestLogger, captures logs to an
// internal buffer and provides a contains() method for checking what
// was logged. Be wary of using this as it leads to test fragility.

// On construction, sets the log level to 99 to avoid false positives.
// Its scope should therefore be kept as small as possible to avoid
// this.

// On destruction, reinstates PrintingTestLogger::DEFAULT as the
// logger. This includes setting the logging level back from 99 to the
// value based on the NOISY environment variable.
class CapturingTestLogger: public PrintingTestLogger
{
public:
  CapturingTestLogger() : PrintingTestLogger()
  {
    setPrinting(PrintingTestLogger::DEFAULT.isPrinting());
    setLoggingLevel(99);
    pthread_mutex_init(&_logger_lock, NULL);
  };

  CapturingTestLogger(int level) : PrintingTestLogger()
  {
    setPrinting(PrintingTestLogger::DEFAULT.isPrinting());
    setLoggingLevel(level);
    pthread_mutex_init(&_logger_lock, NULL);
  };

  virtual ~CapturingTestLogger()
  {
    pthread_mutex_destroy(&_logger_lock);
  };

  void write(const char* line);
  bool contains(const char* fragment);

private:
  std::string _logged;
  pthread_mutex_t _logger_lock;
};

#endif
