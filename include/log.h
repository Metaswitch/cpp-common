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

// The following macro caches the details of the trace call being made and
// stores the associated instance it (the "trace ID") in a static variable so
// that subsequent calls to this trace line can be stored in the RAM trace
// buffer with maximal efficiency.
#define TRC_RAMTRACE(...)                                                     \
{                                                                             \
  static int trc_id = 0;                                                      \
                                                                              \
  Log::ramCacheTrcCall(&trc_id,__FILE__,__LINE__,__VA_ARGS__);                \
}

#define TRC_ERROR(...) TRC_RAMTRACE(__VA_ARGS__) if (Log::enabled(Log::ERROR_LEVEL)) Log::write(Log::ERROR_LEVEL, __FILE__, __LINE__, __VA_ARGS__)
#define TRC_WARNING(...) TRC_RAMTRACE(__VA_ARGS__) if (Log::enabled(Log::WARNING_LEVEL)) Log::write(Log::WARNING_LEVEL, __FILE__, __LINE__, __VA_ARGS__)
#define TRC_STATUS(...) TRC_RAMTRACE(__VA_ARGS__) if (Log::enabled(Log::STATUS_LEVEL)) Log::write(Log::STATUS_LEVEL, __FILE__, __LINE__, __VA_ARGS__)
#define TRC_INFO(...) TRC_RAMTRACE(__VA_ARGS__) if (Log::enabled(Log::INFO_LEVEL)) Log::write(Log::INFO_LEVEL, __FILE__, __LINE__, __VA_ARGS__)
#define TRC_VERBOSE(...) TRC_RAMTRACE(__VA_ARGS__) if (Log::enabled(Log::VERBOSE_LEVEL)) Log::write(Log::VERBOSE_LEVEL, __FILE__, __LINE__, __VA_ARGS__)
#define TRC_DEBUG(...) TRC_RAMTRACE(__VA_ARGS__) if (Log::enabled(Log::DEBUG_LEVEL)) Log::write(Log::DEBUG_LEVEL, __FILE__, __LINE__, __VA_ARGS__)
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

  extern pthread_mutex_t trc_ram_trc_cache_lock;

  void ramCacheTrcCall(int *trc_id, const char *module, int lineno, const char*fmt, ...);

  int ramCacheTrcCall(const char *module, int lineno, const char*fmt, ...);
  void ramTrace(int trc_id, const char *fmt, ...);
  void ramDecode(FILE *output);

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

#endif
