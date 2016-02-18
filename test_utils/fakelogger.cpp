/**
 * @file fakelogger.cpp Fake logger (for testing).
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

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <cassert>

#include "fakelogger.h"

PrintingTestLogger PrintingTestLogger::DEFAULT;

PrintingTestLogger::PrintingTestLogger() : BaseTestLogger()
{
  take_over();
}

PrintingTestLogger::~PrintingTestLogger()
{
  relinquish_control();
}

CapturingTestLogger::CapturingTestLogger() : BaseTestLogger()
{
  take_over();
  setPrinting(PrintingTestLogger::DEFAULT.isPrinting());
  setLoggingLevel(99);
  pthread_mutex_init(&_logger_lock, NULL);
};

CapturingTestLogger::CapturingTestLogger(int level) : BaseTestLogger()
{
  take_over();
  setPrinting(PrintingTestLogger::DEFAULT.isPrinting());
  setLoggingLevel(level);
  pthread_mutex_init(&_logger_lock, NULL);
};

CapturingTestLogger::~CapturingTestLogger()
{
  relinquish_control();
  Log::setLoggingLevel(_last_logging_level);
  pthread_mutex_destroy(&_logger_lock);
};


void BaseTestLogger::take_over()
{
  _last_logging_level = Log::loggingLevel;
  _last_logger = Log::setLogger(this);
  setupFromEnvironment();
}

void BaseTestLogger::relinquish_control()
{
  Log::setLoggingLevel(_last_logging_level);
  Logger* replaced = Log::setLogger(_last_logger);

  // Ensure that loggers are destroyed in the reverse order of creation.
  assert(replaced == this);
}

bool BaseTestLogger::isPrinting()
{
  return _noisy;
}

void BaseTestLogger::setPrinting(bool printing)
{
  _noisy = printing;
}

void BaseTestLogger::setLoggingLevel(int level)
{
  Log::setLoggingLevel(level);
}

void BaseTestLogger::write(const char* data)
{
  std::string line(data);

  if (*line.rbegin() != '\n') {
    line.push_back('\n');
  }

  if (_noisy)
  {
    std::cout << line;
  }

}

void CapturingTestLogger::write(const char* data)
{
  std::string line(data);

  if (*line.rbegin() != '\n') {
    line.push_back('\n');
  }

  // Note that although Log::write ensures that writes are serialised
  // with a lock, we want a separate lock to avoid a conflict with contains().
  pthread_mutex_lock(&_logger_lock);
  _logged.append(line);
  pthread_mutex_unlock(&_logger_lock);

  BaseTestLogger::write(data);
}

bool CapturingTestLogger::contains(const char* fragment)
{
  bool result;
  pthread_mutex_lock(&_logger_lock);
  result = _logged.find(fragment) != std::string::npos;
  pthread_mutex_unlock(&_logger_lock);
  return result;
}

void BaseTestLogger::flush()
{
}

void BaseTestLogger::setupFromEnvironment()
{
  // Set logging to the specified level if specified in the environment.
  // NOISY=T:5 sets the level to 5.
  char* val = getenv("NOISY");
  bool is_noisy = ((val != NULL) &&
                   (strchr("TtYy", val[0]) != NULL));
  int level = DEFAULT_LOGGING_LEVEL;

  if (val != NULL)
  {
    val = strchr(val, ':');

    if (val != NULL)
    {
      level = strtol(val + 1, NULL, 10);
    }
  }

  setPrinting(is_noisy);
  setLoggingLevel(level);
}
