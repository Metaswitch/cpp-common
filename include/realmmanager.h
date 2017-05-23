/**
 * @file realmmanager.h 
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef REALMMANAGER_H__
#define REALMMANAGER_H__

#include <pthread.h>

#include "diameterstack.h"
#include "diameterresolver.h"

class RealmManager
{
public:
  RealmManager(Diameter::Stack* stack,
               std::string realm,
               std::string host,
               int max_peers,
               DiameterResolver* resolver);
  virtual ~RealmManager();

  void start();
  void stop();

  void peer_connection_cb(bool connection_success,
                          const std::string& host,
                          const std::string& realm);

  void srv_priority_cb(struct fd_list* candidates);

  static const int DEFAULT_BLACKLIST_DURATION;

private:
  void thread_function();
  static void* thread_function(void* realm_manager_ptr);

  void manage_connections(int& ttl);

  // We use a read/write lock to read and update the _peers map (defined below).
  // However, we read this map on every single Diameter message, so we want to
  // minimise blocking. Therefore we only grab the write lock when we are ready
  // to write to _peers. This means we may first grab the read lock (to work
  // out what write we want to do). However, we can't upgrade a read lock to a
  // write lock, and we don't want somebody else to write to the peers map
  // whilst in between reading and writing. Therefore a function that wishes to
  // write to the peers map MUST ALSO HAVE HOLD OF THE MAIN THREAD LOCK. This is
  // not policed anywhere (in fact, we can't police it), but that's how these
  // locks should be used.
  pthread_mutex_t _main_thread_lock;
  pthread_rwlock_t _peers_lock;

  Diameter::Stack* _stack;
  std::string _realm;
  std::string _host;
  int _max_peers;
  pthread_t _thread;
  pthread_cond_t _cond;
  DiameterResolver* _resolver;
  std::map<std::string, Diameter::Peer*> _peers;
  volatile bool _terminating;
};

#endif
