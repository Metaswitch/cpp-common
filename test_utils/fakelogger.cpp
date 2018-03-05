/**
 * @file fakelogger.cpp Fake logger (for testing).
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
