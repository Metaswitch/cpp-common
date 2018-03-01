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

class RealmManager
{
public:
  RealmManager(Diameter::Stack* stack,
               std::string realm,
               std::string host,
               unsigned int max_peers,
               DiameterResolver* resolver,
               Alarm* alarm=NULL,
               std::string sender="",
               std::string receiver="");
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

  // utility function that turns the std::map _failed_peers into a csv string
  std::string create_failed_peers_string();

  /// -------------------------- Alarm mechanics -------------------------- ///
  // The _peer_connection_alarm is used to monitor the connection to diameter
  // peers. The parameter controlling the mechanics of the alarm is _max_peers.
  // We keep track of the peers we failed to connect to using the map
  // _failed_peers. We add the failed peer together with a timestamp to this
  // map, so that we can remove stale failed peers which no longer get returned
  // by the DNS (see Step 8 in manage_connections).
  // We raise the alarm if we fail to connect to a peer and the number of
  // connected peers (num_connected_peers) is strictly less than _max_peers.
  // We clear the alarm if, either we successfully connect to a peer and the
  // number of connected peers is at least _max_peers, or there are no failed 
  // peers. This ensures we clear the alarm if the total number of available
  // peers isn less than _max_peers and we successfully connect to all of them.
  // We also make ENT logs specifying the ip addresses of the failed peers,
  // thus helping us diagnose which peers we failed to connect to. 
  // There are three types of ENT logs:
  // 1. partial connection error log (CL_CM_CONNECTION_PARTIAL_ERROR_EXPLICIT),
	// 2. total connection error log (CL_CM_CONNECTION_ERRORED), and
	// 3. connection restored log (CL_CM_CONNECTION_PARTIAL_CLEARED_EXPLICIT).
  // These logs should be self explanatory.
  /// --------------------------------------------------------------------- ///
  void manage_alarm(const bool connected,
                    Diameter::Peer* peer,
                    const int change,
                    const unsigned int num_connected_peers);

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

  Alarm* _peer_connection_alarm;
  Diameter::Stack* _stack;
  std::string _realm;
  std::string _host;
  unsigned int _max_peers;
  pthread_t _thread;
  pthread_cond_t _cond;
  DiameterResolver* _resolver;
  std::map<std::string, Diameter::Peer*> _peers;
  std::map<AddrInfo, const unsigned long> _failed_peers;
  std::string _sender;
  std::string _receiver;
  unsigned long _failed_peers_timeout_ms = 24*3600*1000;
  volatile bool _terminating;
  bool _total_connection_error;
};

#endif
