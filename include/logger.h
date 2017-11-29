/**
 * @file logger.h Definitions for Sprout logger class.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

///
///

#ifndef LOGGER_H__
#define LOGGER_H__

#include <string>
#include <pthread.h>

class Logger
{
public:
  Logger();
  Logger(const std::string& directory, const std::string& filename);
  virtual ~Logger();

  static const int ADD_TIMESTAMPS = 1;
  static const int FLUSH_ON_WRITE = 2;
  int get_flags() const;
  void set_flags(int flags);

  virtual void write(const char* data);
  virtual void flush();
  virtual void commit();

  // Dumps a backtrace.  Note that this is not thread-safe and should only be
  // called when no other threads are running - generally from a signal
  // handler.
  virtual void backtrace(const char* data);

protected:
  virtual void gettime_monotonic(struct timespec* ts);
  virtual void gettime(struct timespec* ts);

private:
  /// Encodes the time as needed by the logger.
  typedef struct
  {
    int year;
    int mon;
    int mday;
    int hour;
    int min;
    int sec;
    int msec;
    int yday;
  } timestamp_t;

  void get_timestamp(timestamp_t& ts);
  void write_log_file(const char* data, const timestamp_t& ts);
  void cycle_log_file(const timestamp_t& ts);

  // Two methods to use with pthread_cleanup_push to release the lock if the logging thread is
  // forcibly killed.
  static void release_lock(void* logger) {((Logger*)logger)->release_lock();}
  void release_lock() {pthread_mutex_unlock(&_lock);}

  int _flags;
  int _last_hour;
  bool _rotate;
  struct timespec _last_rotate;
  FILE* _fd;
  int _discards;
  int _saved_errno;
  std::string _filename;
  std::string _directory;
  pthread_mutex_t _lock;

  /// Defines how frequently (in seconds) we will try to reopen a log
  /// file when we have previously failed to use it.
  const static double LOGFILE_RETRY_FREQUENCY;
};


#endif
