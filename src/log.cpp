/**
 * @file log.cpp
 *
 * Copyright (C) Metaswitch Networks 2014
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */


#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <algorithm>
#include <time.h>
#include "log.h"

const char* log_level[] = {"Error", "Warning", "Status", "Info", "Verbose", "Debug"};

#define MAX_LOGLINE 8192

namespace Log
{
  static Logger logger_static;
  static Logger *logger = &logger_static;
  static pthread_mutex_t serialization_lock = PTHREAD_MUTEX_INITIALIZER;
  int loggingLevel = 4;
}

void Log::setLoggingLevel(int level)
{
  if (level > DEBUG_LEVEL)
  {
    level = DEBUG_LEVEL;
  }
  else if (level < ERROR_LEVEL)
  {
    level = ERROR_LEVEL; // LCOV_EXCL_LINE
  }
  Log::loggingLevel = level;
}

// Note that the caller is responsible for deleting the previous
// Logger if it is allocated on the heap.

// Returns the previous Logger (e.g. so it can be stored off and reset).
Logger* Log::setLogger(Logger *log)
{
  pthread_mutex_lock(&Log::serialization_lock);
  Logger* old = Log::logger;
  Log::logger = log;
  if (Log::logger != NULL)
  {
    Log::logger->set_flags(Logger::FLUSH_ON_WRITE|Logger::ADD_TIMESTAMPS);
  }
  pthread_mutex_unlock(&Log::serialization_lock);
  return old;
}

void Log::write(int level, const char *module, int line_number, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  _write(level, module, line_number, fmt, args);
  va_end(args);
}

static void release_lock(void* notused) { pthread_mutex_unlock(&Log::serialization_lock); } // LCOV_EXCL_LINE

static void log_helper(char* logline,
                int& written,
                int& truncated,
                int level,
                const char *module,
                int line_number,
                const char* context,
                const char *fmt,
                va_list args)
{
  written = 0;
  truncated = 0;

  pthread_t thread = pthread_self();

  written = snprintf(logline, MAX_LOGLINE -2, "[%lx] %s ", thread, log_level[level]);
  int bytes_available = MAX_LOGLINE - written - 2;

  // If no module is supplied then all the information in the log is supplied in the fmt
  // parameter, so we skip to writing fmt.
  if (module != NULL)
  {
    const char* mod = strrchr(module, '/');
    module = (mod != NULL) ? mod + 1 : module;

    if (line_number)
    {
      if (context)
      {
        written += snprintf(logline + written, bytes_available, "%s:%d:%s: ", module, line_number, context);
      }
      else
      {
        written += snprintf(logline + written, bytes_available, "%s:%d: ", module, line_number);
      }
    }
    else
    {
      if (context)
      {
        written += snprintf(logline + written, bytes_available, "%s:%s: ", module, context);
      }
      else
      {
        written += snprintf(logline + written, bytes_available, "%s: ", module);
      }
    }
  }

  // snprintf and vsnprintf return the bytes that would have been
  // written if their second argument was large enough, so we need to
  // reduce the size of written to compensate if it is too large.
  written = std::min(written, MAX_LOGLINE - 1);

  bytes_available = MAX_LOGLINE - written - 1;
  written += vsnprintf(logline + written, bytes_available, fmt, args);

  if (written > (MAX_LOGLINE - 1))
  {
    truncated = written - (MAX_LOGLINE - 2);
    written = MAX_LOGLINE - 2;
  }

  // Add a new line
  logline[written] = '\n';
  written++;
}


void Log::_write(int level, const char *module, int line_number, const char *fmt, va_list args)
{
  if (level > Log::loggingLevel)
  {
    return;
  }

  pthread_mutex_lock(&Log::serialization_lock);
  if (!Log::logger)
  {
    // LCOV_EXCL_START
    pthread_mutex_unlock(&Log::serialization_lock);
    return;
    // LCOV_EXCL_STOP
  }

  pthread_cleanup_push(release_lock, 0);

  char logline[MAX_LOGLINE];
  int written;
  int truncated;

  log_helper(logline, written, truncated, level, module, line_number, nullptr, fmt, args);

  // Add a null termination.
  logline[written] = '\0';

  Log::logger->write(logline);
  if (truncated > 0)
  {
    char buf[128];
    snprintf(buf, 128, "Previous log was truncated by %d characters\n", truncated);
    Log::logger->write(buf);
  }

  pthread_cleanup_pop(0);
  pthread_mutex_unlock(&Log::serialization_lock);
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
  // snprintf and vsnprintf return the bytes that would have been
  // written if their second argument was large enough, so we need to
  // reduce the size of written to compensate if it is too large.
  int written = vsnprintf(logline, MAX_LOGLINE - 2, fmt, args);
  written = std::min(written, MAX_LOGLINE - 2);
  va_end(args);

  // Add a new line and null termination.
  logline[written] = '\n';
  logline[written+1] = '\0';

  Log::logger->backtrace_simple(logline);
}

void Log::backtrace_adv()
{
  if (!Log::logger)
  {
    return;
  }

  Log::logger->backtrace_advanced();
}

void Log::commit()
{
  if (!Log::logger)
  {
    return;
  }

  Log::logger->commit();
}

// LCOV_EXCL_STOP

// 20 MB RAM Bufffer
#define RAM_BUFFER_SIZE 20971520

namespace RamRecorder
{
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

  static char buffer[RAM_BUFFER_SIZE];
  static char* buffer_start = buffer;
  static char* buffer_end = buffer;
  bool record_everything = false;
}

void RamRecorder::recordEverything()
{
  RamRecorder::record_everything = true;
}

void RamRecorder::_record(int level, const char* module, int lineno, const char* context, const char* format, va_list args)
{
  int written;
  int truncated;

  {
    char timestamp[100];
    timestamp_t ts;
    struct timespec timespec;
    clock_gettime(CLOCK_REALTIME, &timespec);
    Logger::get_timestamp(ts, timespec);
    Logger::format_timestamp(ts, timestamp, sizeof(timestamp));
    RamRecorder::write(timestamp, strlen(timestamp));
    RamRecorder::write(" ", 1);
  }

  {
    char logline[MAX_LOGLINE];

    // Fill out the log line
    log_helper(logline, written, truncated, level, module, lineno, context, format, args);

    RamRecorder::write(logline, written);
  }

  if (truncated)
  {
    char buf[128];
    int len = snprintf(buf, 128, "Earlier log was truncated by %d characters\n", truncated);
    RamRecorder::write(buf, len);
  }
}

void RamRecorder::record(int level, const char* module, int lineno, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  _record(level, module, lineno, nullptr, format, args);
  va_end(args);
}

void RamRecorder::record_with_context(int level, const char* module, int lineno, char* context, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  _record(level, module, lineno, context, format, args);
  va_end(args);
}

void RamRecorder::reset()
{
  RamRecorder::record_everything = false;
  pthread_mutex_lock(&RamRecorder::lock);
  buffer_end = buffer;
  buffer_start = buffer;
  pthread_mutex_unlock(&RamRecorder::lock);
}

void RamRecorder::write(const char* message, size_t length)
{
  pthread_mutex_lock(&RamRecorder::lock);
  const char* end = buffer + RAM_BUFFER_SIZE;

  while (length > 0)
  {
    size_t left = end - buffer_end;
    size_t write = std::min(length, left);

    // Write until the end of the buffer
    memcpy(buffer_end, message, write);

    // We've got less to write now
    length -= write;

    // We've also got less in the message buffer to write
    message += write;

    if (write == left)
    {
      // We've reached the end of the buffer.

      // Reset the end of the buffer to cycle round
      char* new_buffer_end = buffer;

      if ((buffer_end < buffer_start) ||
          (buffer_start == new_buffer_end))
      {
        // The end has caught back up with the start,
        // or is about to
        buffer_start = buffer + 1;
      }

      buffer_end = new_buffer_end;
    }
    else
    {
      // There's still some room left. Just move the pointer along
      char* new_buffer_end = buffer_end + write;

      if (buffer_end < buffer_start)
      {
        // The end has cycled round and is catching up with the start

        if (new_buffer_end >= buffer_start)
        {
          // It's caught up with the start. Move the start on
          buffer_start = new_buffer_end + 1;

          if (buffer_start >= end)
          {
            // The start's got to the end. Move it back to the start
            buffer_start = buffer;
          }
        }
      }

      buffer_end = new_buffer_end;
    }
  }

  pthread_mutex_unlock(&RamRecorder::lock);
}

void RamRecorder::dump(const std::string& output_dir)
{
  std::string file_name = output_dir + "/ramtrace." + std::to_string(time(NULL)) + ".txt";

  FILE *file = fopen(file_name.c_str(), "w");

  if (file)
  {
    fprintf(file, "RAM BUFFER\n==========\n");

    pthread_mutex_lock(&RamRecorder::lock);

    if (buffer_end == buffer_start)
    {
      // No bufffered data
      fprintf(file, "No recorded logs\n");
    }
    else if (buffer_end > buffer_start)
    {
      fwrite(buffer_start,
             sizeof(char),
             buffer_end - buffer_start,
             file);
    }
    else
    {
      const char* end = buffer + RAM_BUFFER_SIZE;
      fwrite(buffer_start,
             sizeof(char),
             end - buffer_start,
             file);
      fwrite(buffer,
             sizeof(char),
             buffer_end - buffer,
             file);
    }

    pthread_mutex_unlock(&RamRecorder::lock);

    fprintf(file, "==========\n");

    fclose(file);
  }
  else
  {
    TRC_ERROR("Failed to open file to dump RAM buffer!\n");
  }
}
