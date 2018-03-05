/**
 * @file logger.cpp
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <execinfo.h>
#include <string.h>

// Common STL includes.
#include <cassert>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <queue>
#include <string>

#include "logger.h"

const double Logger::LOGFILE_RETRY_FREQUENCY = 5.0;

Logger::Logger() :
  _flags(ADD_TIMESTAMPS),
  _last_hour(0),
  _rotate(false),
  _last_rotate({0}),
  _fd(stdout)
{
  pthread_mutex_init(&_lock, NULL);
}


Logger::Logger(const std::string& directory, const std::string& filename) :
  _flags(ADD_TIMESTAMPS),
  _last_hour(0),
  _rotate(true),
  _last_rotate({0}),
  _fd(NULL),
  _discards(0),
  _saved_errno(0),
  _filename(filename),
  _directory(directory)
{
  pthread_mutex_init(&_lock, NULL);
}


Logger::~Logger()
{
}


int Logger::get_flags() const
{
  return _flags;
}


void Logger::set_flags(int flags)
{
  _flags = flags;
}


void Logger::gettime(struct timespec* ts)
{
  clock_gettime(CLOCK_REALTIME, ts);
}


void Logger::gettime_monotonic(struct timespec* ts)
{
  clock_gettime(CLOCK_MONOTONIC, ts);
}


/// Writes a log to the logfile, cycling or opening the log file when
/// necessary.
void Logger::write(const char* data)
{
  // Writes logger output to a series of hourly files.
  timestamp_t ts;
  get_timestamp(ts);

  // Take the lock and push a cleanup handler to release it if this thread is
  // forcibly killed while writing to the log file.
  pthread_mutex_lock(&_lock);
  pthread_cleanup_push(Logger::release_lock, this);

  bool cycle_log_file_required = false;

  if (_fd == NULL)
  {
    // When there is no valid log file, try to open one frequently to
    // ensure that as few logs are lost as possible.
    struct timespec current_time;
    gettime_monotonic(&current_time);
    double seconds = difftime(current_time.tv_sec, _last_rotate.tv_sec);

    // Make sure to catch the case when logging is started before the
    // monotonic clock gets higher than LOGFILE_RETRY_FREQUENCY.
    if ((seconds > LOGFILE_RETRY_FREQUENCY) ||
        (current_time.tv_sec < LOGFILE_RETRY_FREQUENCY))
     {
      cycle_log_file_required = true;
    }
  }
  else
  {
    // Convert the date/time into a rough number of hours since some base date.
    // This doesn't have to be exact, but it does have to be monotonically
    // increasing, so assume every year is a leap year.
    int hour = ts.year * 366 * 24 + ts.yday * 24 + ts.hour;
    if (_rotate && (hour > _last_hour))
    {
      cycle_log_file_required = true;
      _last_hour = hour;
    }
  }

  if (cycle_log_file_required)
  {
    // Open a new log file.
    cycle_log_file(ts);
    gettime_monotonic(&_last_rotate);

    if ((_fd != NULL) &&
        (_discards != 0))
    {
      char discard_msg[100];
      sprintf(discard_msg,
              "Failed to open logfile (%d - %s), %d logs discarded\n",
              _saved_errno, ::strerror(_saved_errno), _discards);
      write_log_file(discard_msg, ts);
      _discards = 0;
      _saved_errno = 0;
    }
  }

  if (_fd != NULL)
  {
    // We have a valid log file open, so write the log.
    write_log_file(data, ts);
  }
  else
  {
    // No valid log file, so count this as a discard.
    ++_discards;
  }

  pthread_cleanup_pop(0);
  pthread_mutex_unlock(&_lock);
}


/// Gets a timestamp in the form required by the logger.
void Logger::get_timestamp(timestamp_t& ts)
{
  struct timespec timespec;
  struct tm dt;
  gettime(&timespec);
  gmtime_r(&timespec.tv_sec, &dt);
  ts.year = dt.tm_year;
  ts.mon = dt.tm_mon;
  ts.mday = dt.tm_mday;
  ts.hour = dt.tm_hour;
  ts.min = dt.tm_min;
  ts.sec = dt.tm_sec;
  ts.msec = (int)(timespec.tv_nsec / 1000000);
  ts.yday = dt.tm_yday;
}

void Logger::format_timestamp(const timestamp_t& ts, char* buf, size_t len)
{
  snprintf(buf, len,
           "%2.2d-%2.2d-%4.4d %2.2d:%2.2d:%2.2d.%3.3d UTC",
           ts.mday, (ts.mon+1), (ts.year + 1900),
           ts.hour, ts.min, ts.sec, ts.msec);
}

/// Writes a log to the file with timestamp if configured.
void Logger::write_log_file(const char *data, const timestamp_t& ts)
{
  if (_flags & ADD_TIMESTAMPS)
  {
    char timestamp[100];
    format_timestamp(ts, timestamp, sizeof(timestamp));
    fprintf(_fd, "%s ", timestamp);
  }

  // Write the log to the current file.
  fputs(data, _fd);

  if (_flags & FLUSH_ON_WRITE)
  {
    fflush(_fd);
  }

  if (ferror(_fd))
  {
    // LCOV_EXCL_START
    fclose(_fd);
    _fd = NULL;
    // LCOV_EXCL_STOP
  }
}


void Logger::cycle_log_file(const timestamp_t& ts)
{
  if (_fd != NULL)
  {
    fclose(_fd);
  }

  std::string prefix = _directory + "/" + _filename + "_";
  char time_date_stamp[100];
  sprintf(time_date_stamp, "%4.4d%2.2d%2.2dT%2.2d0000Z",
          (ts.year + 1900),
          (ts.mon + 1),
          ts.mday,
          ts.hour);
  std::string full_path = prefix + time_date_stamp + ".txt";

  _fd = fopen(full_path.c_str(), "a");

  // Set up /var/log/<component>/<component>_current.txt as a symlink
  // If this fails, there's not much we can do, it's not like we can drop
  // a log.
  std::string symlink_path = prefix + "current.txt";
  std::string relative_path = "./" + _filename + "_" + time_date_stamp + ".txt";
  unlink(symlink_path.c_str());

  if (symlink(relative_path.c_str(), symlink_path.c_str()) < 0)
  {
    // We don't get a helpful symlink.
  }

  if (_fd == NULL)
  {
    // Failed to open logfile, so save errno until we can log it.
    _saved_errno = errno;
  }
}


// LCOV_EXCL_START Only used in exceptional signal handlers - not hit in UT

// Maximum number of stack entries to trace out.
#define MAX_BACKTRACE_STACK_ENTRIES 32

// Dump a simple backtrace (using functionality available from within the
// process). This is fast but not very good - in particular it doesn't print out
// function names or arguments.
//
// This function is called from a signal handler and so can
// only use functions that are safe to be called from one.  In particular,
// locking functions are _not_ safe to call from signal handlers, so this
// function is not thread-safe.
void Logger::backtrace_simple(const char* data)
{
  // If the file exists, dump a header and then the backtrace.
  if (_fd != NULL)
  {
    fprintf(_fd, "\n%s", data);
    fprintf(_fd, "\nBasic stack dump:\n");
    fflush(_fd);
    void *stack[MAX_BACKTRACE_STACK_ENTRIES];
    size_t num_entries = ::backtrace(stack, MAX_BACKTRACE_STACK_ENTRIES);
    backtrace_symbols_fd(stack, num_entries, fileno(_fd));
    fprintf(_fd, "\n");

    fflush(_fd);

    if (ferror(_fd))
    {
      fclose(_fd);
      _fd = NULL;
    }
  }
}

// Dump an advanced backtrace using GDB. This captures function names and
// arguments but stops the process temporarily. Because of this it should only
// be used when the process is about to exit.
//
// This function is called from a signal handler and so can only use functions
// that are safe to be called from one.  In particular, locking functions are
// _not_ safe to call from signal handlers, so this function is not thread-safe.
void Logger::backtrace_advanced()
{
  // If the file exists, dump a header and then the backtrace.
  if (_fd != NULL)
  {
    // Dumping might not work (e.g. because gdb isn't installed), but it gives
    // much better output.  We need to swap some file descriptors around before
    // and after invoking gdb to make sure that stdout and stderr from gdb go to
    // stderr.
    int fd1 = dup(1);
    dup2(2, 1);

   // Get a timestamp so we can put that into stderr to correlate with the other
   // logs in the logfile.
    timestamp_t ts;
    char timestamp[100];
    get_timestamp(ts);
    format_timestamp(ts, timestamp, sizeof(timestamp));

    // Dump the stack trace to stderr. Given we are writing to a file from two
    // processes, we flush our buffers after writing to stderr to minimize the
    // chance of any re-ordering.
    fprintf(stderr, "\n%s Advanced stack dump (requires gdb):\n", timestamp); fflush(stderr);

    char gdb_cmd[256];
    sprintf(gdb_cmd,
            "/usr/bin/gdb -nx --batch /proc/%d/exe %d -ex 'thread apply all bt'",
            getpid(),
            getpid());
    int rc = system(gdb_cmd);

    if (rc != 0)
    {
      fprintf(stderr, "gdb failed with return code %d\n", rc); fflush(stderr);
    }

    // Put the file descriptors back the way they were.
    dup2(fd1, 1);
    close(fd1);

    // Also print a message to the logfile saying what just happened.
    if (rc != 0)
    {
      fprintf(_fd, "\nAdvanced stack dump failed: gdb returned %d\n\n", rc);
    }
    else
    {
      fprintf(_fd, "\nAdvanced stack dump written to stderr (with timestamp %s)\n\n",
              timestamp);
    }

    fflush(_fd);

    if (ferror(_fd))
    {
      fclose(_fd);
      _fd = NULL;
    }
  }
}

void Logger::commit()
{
  fsync(fileno(_fd));
}

// LCOV_EXCL_STOP


void Logger::flush()
{
  fflush(_fd);
}
