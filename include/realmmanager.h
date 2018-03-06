/**
 * @file realmmanager.h 
 *
 * Copyright (C) Metaswitch Networks 2015
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
#include "pdlog.h"

class RealmManager
{
public:
  RealmManager(Diameter::Stack* stack,
               std::string realm,
               std::string host,
               int max_peers,
               DiameterResolver* resolver,
               Alarm* alarm,
               const PDLog* ALARM_CLEAR_LOG,
               const PDLog1<const char*>* ALARM_ERROR_LOG);
  virtual ~RealmManager();

  void start();
  void stop();

  void peer_connection_cb(bool connection_success,
                          const std::string& host,
                          const std::string& realm);

  void srv_priority_cb(struct fd_list* candidates);

  static const int DEFAULT_BLACKLIST_DURATION;
  static const unsigned int FAILED_PEERS_TIMEOUT_MS = 24*3600*1000;

private:
  void thread_function();
  static void* thread_function(void* realm_manager_ptr);

  void manage_connections(int& ttl);
  void monitor_connections(const bool& failed_peers_changed);

  // Utility function that turns the std::map _failed_peers into a CSV string
  std::string create_failed_peers_string();
  
  // Helper functions that modify the _failed_peers map. Both return true if
  // modified, false otherwise.
  bool add_to_failed_peers(Diameter::Peer* peer);
  bool remove_from_failed_peers(Diameter::Peer* peer);

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
  Alarm* _peer_connection_alarm;
  std::map<std::string, Diameter::Peer*> _peers;
  std::map<AddrInfo, const unsigned long> _failed_peers;
  const PDLog* _ALARM_CLEAR_LOG;
  const PDLog1<const char*>* _ALARM_ERROR_LOG;
  volatile bool _terminating;
};
#endif
