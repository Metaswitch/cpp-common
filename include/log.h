/**
 * @file log.h
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */


#ifndef LOG_H__
#define LOG_H__

#include "logger.h"
#include <cstdarg>

#define TRC_RAMTRACE(level, ...) RamRecorder::record(level, __FILE__, __LINE__, ##__VA_ARGS__)

#define TRC_MAYBE_RAMTRACE(...)                                            \
do {                                                                       \
  if (RamRecorder::record_everything) {                                    \
    TRC_RAMTRACE(__VA_ARGS__);                                             \
  }                                                                        \
} while (0)

#define TRC_LOG(level, ...) if (Log::enabled(level)) Log::write(level, __FILE__, __LINE__, ##__VA_ARGS__)
#define TRC_BASE(level, ...) do { TRC_MAYBE_RAMTRACE(level, ##__VA_ARGS__); TRC_LOG(level, ##__VA_ARGS__); } while (0)

#define TRC_ERROR(...) TRC_BASE(Log::ERROR_LEVEL, ##__VA_ARGS__)
#define TRC_WARNING(...) TRC_BASE(Log::WARNING_LEVEL, ##__VA_ARGS__)
#define TRC_STATUS(...) TRC_BASE(Log::STATUS_LEVEL, ##__VA_ARGS__)
#define TRC_INFO(...) TRC_BASE(Log::INFO_LEVEL, ##__VA_ARGS__)
#define TRC_VERBOSE(...) TRC_BASE(Log::VERBOSE_LEVEL, ##__VA_ARGS__)
#define TRC_DEBUG(...) TRC_BASE(Log::DEBUG_LEVEL, ##__VA_ARGS__)

#define TRC_BACKTRACE(...) Log::backtrace(__VA_ARGS__)
#define TRC_BACKTRACE_ADV() Log::backtrace_adv()
#define TRC_COMMIT() Log::commit()

namespace Log
{
  const int ERROR_LEVEL = 0;
  const int WARNING_LEVEL = 1;
  const int STATUS_LEVEL = 2;
  const int INFO_LEVEL = 3;
  const int VERBOSE_LEVEL = 4;
  const int DEBUG_LEVEL = 5;

  extern int loggingLevel;

  inline bool enabled(int level)
  {
#ifdef UNIT_TEST
    // Always force log parameter evaluation for unit tests
    return true;
#else
    return (level <= loggingLevel);
#endif
  }
  void setLoggingLevel(int level);
  Logger* setLogger(Logger *log);
  void write(int level, const char *module, int line_number, const char *fmt, ...);
  void _write(int level, const char *module, int line_number, const char *fmt, va_list args);
  void backtrace(const char* fmt, ...);
  void backtrace_adv();
  void commit();
}

namespace RamRecorder
{
  extern bool record_everything;
  void recordEverything();
  void _record(int level, const char* module, int lineno, const char* context, const char* format, va_list args);
  void record(int level, const char* module, int lineno, const char* format, ...);
  void record_with_context(int level, const char* module, int lineno, char* context, const char* format, ...);
  void write(const char* buffer, size_t length);
  void dump(const std::string& output_dir);
}

#endif
