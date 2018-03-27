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
  //
  // The following helper functions (that begin with an underscore) are
  // logically private and should not be called directly.
  //

  void _write_var(int level, const char *module, int line_number, const char *fmt, ...);
  void _write(int level, const char *module, int line_number, const char *fmt, va_list args);

  // Functions that convert a value into a version that is suitable for logging.
  // For most types this is just the identity function (i.e. it passes the value
  // through) and if this is not an acceptable type we get a compilation error.
  //
  // The exception is `std::string`, where we specialize the template to convert
  // it to a `const char*` by calling c_str(). This is safe because the caller
  // of `_loggable` always passes it an lvalue which exists for the duration of
  // the log call.
  //
  // Note that we need template parameters for both the argument and the return
  // type so that the specialization for std::string can return a type that's
  // different from the argument.
  template<class T, class R=T>
  R _loggable(T arg) { return arg; }

  template<class R=const char*>
  const char* _loggable(const std::string& arg) { return arg.c_str(); }

  //
  // Constants and variables after this point are public.
  //

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

  // write function that is parameterized over all types that may be passed to
  // it. This converts these arguments into their loggable form before calling
  // the simple variadic `_write_var` function.
  template<typename ...Types>
  void write(int level, const char *module, int line_number, const char *fmt, Types... args)
  {
    // C++11 suport parameter packs which are lists of parameters of (possibly)
    // heterogeneous types. The expression `_loggable(args)...` expands to
    // calling `_loggable` on each argument.
    //
    // For example if args contains two arguments (arg0 and arg1) this expands
    // to `_loggable(arg0), _loggable(arg1)`, preserving the type information
    // for arg0 and arg1.
    _write_var(level, module, line_number, fmt, _loggable(args)...);
  }

  void backtrace(const char* fmt, ...);
  void backtrace_adv();
  void commit();
}

namespace RamRecorder
{
  extern bool record_everything;
  void _record(int level,
               const char* module,
               int lineno,
               const char* context,
               const char* format,
               va_list args);

  void recordEverything();
  void reset();
  void write(const char* buffer, size_t length);
  void dump(const std::string& output_dir);

  /// Record methods that work on plain-old-data types (e.g. chars, ints,
  /// pointers). The caller is responsible for ensuring that any arguments
  /// passed into these functions are valid until the function returns. For
  /// example, it is not safe to call `c_str` on a string rvalue and pass the
  /// resulting `char*` into one of these functions.

  void record_pod(int level,
                  const char* module,
                  int lineno,
                  const char* format,
                  ...);
  void record_with_context_pod(int level,
                               const char* module,
                               int lineno,
                               const char* context,
                               const char* format,
                               ...);

  /// Template parameterized record functions. These use the same approach as the
  /// Log::write function.

  template<typename ...Types>
  void record(int level,
              const char* module,
              int lineno,
              const char* format,
              Types... args)
  {
    record_pod(level, module, lineno, format, Log::_loggable(args)...);
  }

  template<typename ...Types>
  void record_with_context(int level,
                           const char* module,
                           int lineno,
                           const char* context,
                           const char* format,
                           Types... args)
  {
    record_with_context_pod(level, module, lineno, context, format, Log::_loggable(args)...);
  }
}

#endif
