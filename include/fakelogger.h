/**
 * @file fakelogger.hpp Header file for fake logger (for testing).
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
