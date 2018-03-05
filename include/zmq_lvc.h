/**
 * @file zmq_lvc.h
 *
 * Copyright (C) Metaswitch Networks 2014
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef STATISTIC_H__
#define STATISTIC_H__

#include <map>
#include <vector>
#include <string>
#include <zmq.h>

#define ZMQ_NEW_SUBSCRIPTION_MARKER 1

// This folder has to exist and be writable by the running process.
#define ZMQ_IPC_FOLDER_PATH "/var/run/clearwater/stats/"

class LastValueCache
{
public:
  /// Standard constructor
  ///
  /// @param   statcount - The number of statistics to connect to.
  /// @param   statnames - An array of statistics to connect to.
  /// @param   process_name - The name of the current process.
  /// @param   poll_timeout_ms - Used to shorten poll times in UT.
  LastValueCache(int statcount,
                 const std::string *statnames,
                 std::string process_name,
                 long poll_timeout_ms = 1000);
  ~LastValueCache();

  /// Retrive the publish socket for a given statistic.  Statistics
  /// generators can use this to report changes.
  ///
  /// @param  statname - The statistic to be reported.
  /// @returns         - A connected 0mq XPUB socket.
  void* get_internal_publisher(std::string statname);

  /// Call to start the processing on the current thread.
  ///
  /// This is a blocking call.
  void run();

private:
  void clear_cache(void *entry);
  void replay_cache(void *entry);

  void **_subscriber;
  void *_publisher;
  std::map<void *, std::vector<zmq_msg_t *>> _cache;
  pthread_t _cache_thread;
  void *_context;
  int _statcount;
  const std::string *_statnames;
  std::string _process_name;
  const long _poll_timeout_ms;
  volatile bool _terminate;

  /// A bound 0MQ socket per statistic, for use by the internal
  // publishers. At most one thread may use each socket at
  // a time.
  std::map<std::string, void*> _internal_publishers;

  static void* last_value_cache_entry_func(void *);
};

#endif
