/**
 * @file memcachedstore.h Declarations for MemcachedStore class.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */


#ifndef MEMCACHEDSTORE_H__
#define MEMCACHEDSTORE_H__

#include <pthread.h>

#include <sstream>
#include <vector>

extern "C" {
#include <libmemcached/memcached.h>
#include <libmemcached/util.h>
}

#include "store.h"
#include "memcached_config.h"
#include "memcachedstoreview.h"
#include "updater.h"
#include "sas.h"
#include "sasevent.h"
#include "communicationmonitor.h"
#include "astaire_resolver.h"
#include "memcached_connection_pool.h"

class BaseMemcachedStore : public Store
{
public:
  virtual ~BaseMemcachedStore();

  // Some MemcachedStores have their own implementation of this method for
  // working out whether there are any servers configured. However, by default
  // we expect there to be servers.
  bool has_servers() { return true; };

protected:
  // Whether this store is using the binary protocol (required for vbucket
  // support).
  bool _binary;

  // The options string used to create appropriate memcached_st's for the
  // current view.
  std::string _options;

  // The time to wait before timing out a connection to memcached.
  // (This is only used during normal running - at start-of-day we use
  // a fixed 10ms time, to start up as quickly as possible).
  unsigned int _max_connect_latency_ms;

  // The maximum expiration delta that memcached expects.  Any expiration
  // value larger than this is assumed to be an absolute rather than relative
  // value.  This matches the REALTIME_MAXDELTA constant defined by memcached.
  static const int MEMCACHED_EXPIRATION_MAXDELTA = 60 * 60 * 24 * 30;

  // Helper used to track replica communication state, and issue/clear alarms
  // based upon recent activity.
  BaseCommunicationMonitor* _comm_monitor;

  // The lifetime (in seconds) of tombstones that are written to memcached when
  // a record is deleted using `delete_data`. This is needed to allow active
  // resync to spot records that have been deleted since the resync has begun.
  //
  // If this is set to zero the store will actually delete data in memcached
  // instead of using tombstones.
  int _tombstone_lifetime;

  // Constructor. This is protected to prevent the BaseMemcachedStore from being
  // instantiated directly.
  BaseMemcachedStore(bool binary,
                     BaseCommunicationMonitor* comm_monitor);

  // Perform a get request to a single replica.
  memcached_return_t get_from_replica(memcached_st* replica,
                                      const char* key_ptr,
                                      const size_t key_len,
                                      std::string& data,
                                      uint64_t& cas);

  // Add a record to memcached. This overwrites any tombstone record already
  // stored, but fails if any real data is stored.
  memcached_return_t add_overwriting_tombstone(memcached_st* replica,
                                               const char* key_ptr,
                                               const size_t key_len,
                                               const uint32_t vbucket,
                                               const std::string& data,
                                               time_t memcached_expiration,
                                               uint32_t flags,
                                               SAS::TrailId trail);

  // Construct a fully qualified key from the specified table and key within
  // that table.
  static inline std::string get_fq_key(const std::string& table,
                                       const std::string& key)
  {
    return table + "\\\\" + key;
  }
};


/// @class TopologyAwareMemcachedStore
///
/// A memcached-based implementation of the Store class, which knows about the
/// full cross-site topology of the cluster and places keys on exactly the right
/// memcached.
class TopologyAwareMemcachedStore : public BaseMemcachedStore
{
public:
  /// Construct a MemcachedStore that reads its config from a user-supplied
  /// object.
  ///
  /// @param binary        - Whether to use the binary or text interface to
  ///                        memcached.
  /// @param config_file   - A MemcachedConfigReader that the store will use to
  ///                        fetch its config. The store takes ownership of
  ///                        this object and is responsible for freeing it.
  /// @param comm_monitor  - Object tracking memcached communications.
  /// @param vbucket_alarm - Alarm object to kick if a vbucket is
  ///                        uncontactable.
  TopologyAwareMemcachedStore(bool binary,
                              MemcachedConfigReader* config_reader,
                              BaseCommunicationMonitor* comm_monitor = NULL,
                              Alarm* vbucket_alarm = NULL);

  /// Construct a MemcachedStore that reads its config from a file.
  ///
  /// @param binary        - Whether to use the binary or text interface to
  ///                        memcached.
  /// @param config_file   - The file (name and path) to read the config from.
  /// @param comm_monitor  - Object tracking memcached communications.
  /// @param vbucket_alarm - Alarm object to kick if a vbucket is
  ///                        uncontactable.
  TopologyAwareMemcachedStore(bool binary,
                              const std::string& config_file,
                              BaseCommunicationMonitor* comm_monitor = NULL,
                              Alarm* vbucket_alarm = NULL):
    TopologyAwareMemcachedStore(binary,
                                new MemcachedConfigFileReader(config_file),
                                comm_monitor,
                                vbucket_alarm) {}


  ~TopologyAwareMemcachedStore();

  bool has_servers() { return (_servers.size() > 0); };

  /// Flags that the store should use a new view of the memcached cluster to
  /// distribute data.  Note that this is public because it is called from
  /// the MemcachedStoreUpdater class and from UT classes.
  void new_view(const MemcachedConfig& config);

  /// Updates the cluster settings
  void update_config();

  /// Gets the data for the specified table and key.
  Store::Status get_data(const std::string& table,
                         const std::string& key,
                         std::string& data,
                         uint64_t& cas,
                         SAS::TrailId trail = 0);

  /// Sets the data for the specified table and key.
  Store::Status set_data(const std::string& table,
                         const std::string& key,
                         const std::string& data,
                         uint64_t cas,
                         int expiry,
                         SAS::TrailId trail = 0);

  /// Deletes the data for the specified table and key.
  Store::Status delete_data(const std::string& table,
                            const std::string& key,
                            SAS::TrailId trail = 0);

private:
  // Stores the number of vbuckets being used.  This currently doesn't change,
  // but in future we may choose to increase it when the cluster gets
  // sufficiently large.  Note that it _must_ be a power of two.
  const int _vbuckets;

  // Stores the number of replicas configured for the store (one means the
  // data is stored on one server, two means it is stored on two servers etc.).
  const int _replicas;

  // The current global view number.  Note that this is not protected by the
  // _view_lock.
  uint64_t _view_number;

  // The lock used to protect the view parameters below (_servers,
  // _read_replicas and _write_replicas).
  pthread_rwlock_t _view_lock;

  // The list of servers in this view.
  std::vector<std::string> _servers;

  // The set of read and write replicas for each vbucket.
  std::vector<std::vector<std::string> > _read_replicas;
  std::vector<std::vector<std::string> > _write_replicas;

  // A copy of this structure is maintained for each worker thread, as
  // thread local data.
  typedef struct connection
  {
    // Indicates the view number being used by this thread.  When the view
    // changes the global view number is updated and each thread switches to
    // the new view by establishing new memcached_st's.
    uint64_t view_number;

    // Contains the memcached_st's for each server.
    std::map<std::string, memcached_st*> st;

    // Contains the set of read and write replicas for each vbucket.
    std::vector<std::vector<memcached_st*> > write_replicas;
    std::vector<std::vector<memcached_st*> > read_replicas;

  } connection;

  /// Returns the vbucket for a specified key.
  int vbucket_for_key(const std::string& key);

  /// Gets the set of connections to use for a read or write operation.
  typedef enum {READ, WRITE} Op;
  const std::vector<memcached_st*>& get_replicas(const std::string& key, Op operation);
  const std::vector<memcached_st*>& get_replicas(int vbucket, Op operation);

  /// Used to set the communication state for a vbucket after a get/set.
  typedef enum {OK, FAILED} CommState;
  void update_vbucket_comm_state(int vbucket, CommState state);

  // Called by the thread-local-storage clean-up functions when a thread ends.
  static void cleanup_connection(void* p);

  // Used to store a connection structure for each worker thread.
  pthread_key_t _thread_local;

  // State of last communication with replica(s) for a given vbucket, indexed
  // by vbucket.
  std::vector<CommState> _vbucket_comm_state;

  // Number of vbuckets for which the previous get/set failed to contact any
  // replicas (i.e. count of FAILED entries in _vbucket_comm_state).
  unsigned int _vbucket_comm_fail_count;

  // Lock to synchronize access to vbucket comm state accross worker threads.
  pthread_mutex_t _vbucket_comm_lock;

  // Alarms to be used for reporting vbucket inaccessible conditions.
  Alarm* _vbucket_alarm;

  // Object used to read the memcached config.
  MemcachedConfigReader* _config_reader;

  // Stores a pointer to an updater object
  Updater<void, TopologyAwareMemcachedStore>* _updater;

  // Delete a record from memcached by sending a DELETE command.
  void delete_without_tombstone(const std::string& fqkey,
                                const std::vector<memcached_st*>& replicas,
                                SAS::TrailId trail);

  // Delete a record from memcached by writing a tombstone record.
  void delete_with_tombstone(const std::string& fqkey,
                             const std::vector<memcached_st*>& replicas,
                             SAS::TrailId trail);
};


/// @class TopologyNeutralMemcachedStore
///
/// A memcached-based implementation of the Store class, which does not know
/// about the full cross-site topology of the cluster and relies on a
/// topology-aware memcached proxy.
class TopologyNeutralMemcachedStore : public BaseMemcachedStore
{
public:
  /// Construct a MemcachedStore talking to localhost.
  ///
  /// @param target_domain - The domain name for the topology aware proxies.
  /// @param resolver      - The resolver to use to lookup targets in the
  ///                        specified domain.
  /// @param comm_monitor  - Object tracking memcached communications.
  TopologyNeutralMemcachedStore(const std::string& target_domain,
                                AstaireResolver* resolver,
                                BaseCommunicationMonitor* comm_monitor = NULL);

  ~TopologyNeutralMemcachedStore() {}

  /// Gets the data for the specified table and key.
  Store::Status get_data(const std::string& table,
                         const std::string& key,
                         std::string& data,
                         uint64_t& cas,
                         SAS::TrailId trail = 0);

  /// Sets the data for the specified table and key.
  Store::Status set_data(const std::string& table,
                         const std::string& key,
                         const std::string& data,
                         uint64_t cas,
                         int expiry,
                         SAS::TrailId trail = 0);

  /// Deletes the data for the specified table and key.
  Store::Status delete_data(const std::string& table,
                            const std::string& key,
                            SAS::TrailId trail = 0);

protected:
  // The domain name for the memcached proxies.
  std::string _target_domain;

  // Object that can be used to resolve the above domain.
  AstaireResolver* _resolver;

  // How many time to retry a memcached operation.
  const size_t _attempts;

  MemcachedConnectionPool _conn_pool;

  // Determine if for a given memcached return code it is worth retrying a
  // request to a different server in the domain.
  static bool can_retry_memcached_rc(memcached_return_t rc);

  // Get the targets for the configured domain.
  bool get_targets(std::vector<AddrInfo>& targets, SAS::TrailId trail);

  // Call a particular subroutine on each target, stopping if any request gives
  // a definitive result (i.e. a result which means it is not worth trying to a
  // different target).
  //
  // The subroutine should take exactly one parameter - a `ConnectionHandle`
  // which represents the connection to the current target.
  //
  // The following code shows an example of setting a value in memcached. Note
  // that the lambda captures all the values it needs (`key` and `data`) by
  // reference (the `[&]` part). This avoids them being copied, and allows them
  // to be mutated from within the lambda (useful when performing get requests).
  // The connection is not known when the lambda is created and is passed in as
  // a parameter (the `(ConnectionHandle& conn)` part).
  //
  //     std::string key = "Kermit";
  //     std::string data = "The Frog"
  //
  //     iterate_through_targets(targets, trail, [&](ConnectionHandle& conn) {
  //        return memcached_set(conn.get()->st,
  //                             key.data(),
  //                             key.length(),
  //                             data.data(),
  //                             data.length(),
  //                             0,
  //                             0);
  //     });
  //
  // @param targets - The vector of targets to try.
  // @param trail   - SAS trail ID.
  // @param fn      - The subroutine to call on each target.
  memcached_return_t iterate_through_targets(
    std::vector<AddrInfo>& targets,
    SAS::TrailId trail,
    std::function<memcached_return_t(ConnectionHandle<memcached_st*>&)> fn);
};

// Preserve the old name for backwards compatibility
typedef TopologyAwareMemcachedStore MemcachedStore;

#endif
