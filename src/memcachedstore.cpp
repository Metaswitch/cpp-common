/**
 * @file memcachedstore.cpp Memcached-backed implementation of the registration data store.
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


// Common STL includes.
#include <cassert>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <queue>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <time.h>

#include "log.h"
#include "utils.h"
#include "updater.h"
#include "memcachedstoreview.h"
#include "memcachedstore.h"


/// The data used in memcached to represent a tombstone.
static const std::string TOMBSTONE = "";

/// The length of time to allow for a memcached connection before
/// timing it out. This needs to be larger for remote sites.
static int LOCAL_MEMCACHED_CONNECTION_LATENCY_MS = 50;
static int REMOTE_MEMCACHED_CONNECTION_LATENCY_MS = 250;

BaseMemcachedStore::BaseMemcachedStore(bool binary,
                                       BaseCommunicationMonitor* comm_monitor,
                                       bool remote_store) :
  _binary(binary),
  _options(),
  _max_connect_latency_ms(remote_store ? REMOTE_MEMCACHED_CONNECTION_LATENCY_MS :
                                         LOCAL_MEMCACHED_CONNECTION_LATENCY_MS),
  _comm_monitor(comm_monitor),
  _tombstone_lifetime(200)
{
  // Set up the fixed options for memcached.  We use a very short connect
  // timeout because libmemcached tries to connect to all servers sequentially
  // during start-up, and if any are not up we don't want to wait for any
  // significant length of time.
  _options = "--CONNECT-TIMEOUT=10 --SUPPORT-CAS --POLL-TIMEOUT=250";
  _options += (_binary) ? " --BINARY-PROTOCOL" : "";
}


BaseMemcachedStore::~BaseMemcachedStore()
{
}

memcached_return_t BaseMemcachedStore::get_from_replica(memcached_st* replica,
                                                        const char* key_ptr,
                                                        const size_t key_len,
                                                        std::string& data,
                                                        uint64_t& cas)
{
  memcached_return_t rc = MEMCACHED_ERROR;
  cas = 0;

  // We must use memcached_mget because memcached_get does not retrieve CAS
  // values.
  rc = memcached_mget(replica, &key_ptr, &key_len, 1);

  if (memcached_success(rc))
  {
    // memcached_mget command was successful, so retrieve the result.
    TRC_DEBUG("Fetch result");
    memcached_result_st result;
    memcached_result_create(replica, &result);
    memcached_fetch_result(replica, &result, &rc);

    if (memcached_success(rc))
    {
      // Found a record, so exit the read loop.
      TRC_DEBUG("Found record on replica");

      // Copy the record into a string. std::string::assign copies its
      // arguments when used with a char*, so we can free the result
      // afterwards.
      data.assign(memcached_result_value(&result),
                  memcached_result_length(&result));
      cas = memcached_result_cas(&result);
    }

    memcached_result_free(&result);
  }

  return rc;
}


memcached_return_t BaseMemcachedStore::add_overwriting_tombstone(memcached_st* replica,
                                                                 const char* key_ptr,
                                                                 const size_t key_len,
                                                                 const uint32_t vbucket,
                                                                 const std::string& data,
                                                                 time_t memcached_expiration,
                                                                 uint32_t flags,
                                                                 SAS::TrailId trail)
{
  memcached_return_t rc;
  uint64_t cas = 0;

  TRC_DEBUG("Attempting to add data for key %.*s", key_len, key_ptr);

  // Convert the key into a std::string (sas-client does not like that
  // key_{ptr,len} are constant).
  const std::string key(key_ptr, key_len);

  while (true)
  {
    if (cas == 0)
    {
      TRC_DEBUG("Attempting memcached ADD command");
      rc = memcached_add_vb(replica,
                            key_ptr,
                            key_len,
                            _binary ? vbucket : 0,
                            data.data(),
                            data.length(),
                            memcached_expiration,
                            flags);
    }
    else
    {
      TRC_DEBUG("Attempting memcached CAS command (cas = %d)", cas);
      rc = memcached_cas_vb(replica,
                            key_ptr,
                            key_len,
                            _binary ? vbucket : 0,
                            data.data(),
                            data.length(),
                            memcached_expiration,
                            flags,
                            cas);
    }

    if ((rc == MEMCACHED_DATA_EXISTS) ||
        (rc == MEMCACHED_NOTSTORED))
    {
      // A record with this key already exists. If it is a tombstone, we need
      // to overwrite it. Get the record to see what it is.
      memcached_return_t get_rc;
      std::string existing_data;

      TRC_DEBUG("Existing data prevented the ADD/CAS."
                "Issue GET to see if we need to overwrite a tombstone");
      get_rc = get_from_replica(replica, key_ptr, key_len, existing_data, cas);

      if (memcached_success(get_rc))
      {
        if (existing_data != TOMBSTONE)
        {
          // The existing record is not a tombstone.  We mustn't overwrite
          // this, so break out of the loop and return the original return code
          // from the ADD/CAS.
          TRC_DEBUG("Found real data. Give up");
          break;
        }
        else
        {
          // The existing record IS a tombstone. Go round the loop again to
          // overwrite it. `cas` has been set to the cas of the tombstone.
          TRC_DEBUG("Found a tombstone. Attempt to overwrite");

          if (trail != 0)
          {
            SAS::Event event(trail, SASEvent::MEMCACHED_SET_BLOCKED_BY_TOMBSTONE, 0);
            event.add_var_param(key);
            event.add_static_param(cas);
            SAS::report_event(event);
          }
        }
      }
      else if (get_rc == MEMCACHED_NOTFOUND)
      {
        // The GET returned that there is no record for this key. This can
        // happen if the record has expired. We need to try again (it could
        // have been a tombstone which should not block adds).
        TRC_DEBUG("GET failed with NOT_FOUND");

        if (trail != 0)
        {
          SAS::Event event(trail, SASEvent::MEMCACHED_SET_BLOCKED_BY_EXPIRED, 0);
          event.add_var_param(key);
          SAS::report_event(event);
        }
      }
      else
      {
        // The replica failed. Return the return code from the original ADD/CAS.
        TRC_DEBUG("GET failed, rc = %d (%s)\n%s",
                  get_rc,
                  memcached_strerror(replica, get_rc),
                  memcached_last_error_message(replica));
        break;
      }
    }
    else
    {
      TRC_DEBUG("ADD/CAS returned rc = %d (%s)\n%s",
                rc,
                memcached_strerror(replica, rc),
                memcached_last_error_message(replica));
      break;
    }
  }

  return rc;
}


//
// TopologyAwareMemcachedStore method definitions.
//

// LCOV_EXCL_START - TODO

// Constructor.
TopologyAwareMemcachedStore::TopologyAwareMemcachedStore(bool binary,
                                                         MemcachedConfigReader* config_reader,
                                                         bool remote_store,
                                                         BaseCommunicationMonitor* comm_monitor,
                                                         Alarm* vbucket_alarm) :
  BaseMemcachedStore(binary, comm_monitor, remote_store),
  _vbuckets(128),
  _replicas(2),
  _view_number(0),
  _servers(),
  _read_replicas(_vbuckets),
  _write_replicas(_vbuckets),
  _vbucket_comm_state(_vbuckets),
  _vbucket_comm_fail_count(0),
  _vbucket_alarm(vbucket_alarm),
  _config_reader(config_reader),
  _updater(NULL)
{
  // Create the thread local key for the per thread data.
  pthread_key_create(&_thread_local,
                     TopologyAwareMemcachedStore::cleanup_connection);

  // Create the lock for protecting the current view.
  pthread_rwlock_init(&_view_lock, NULL);

  // Create the mutex for protecting vbucket comm state.
  pthread_mutex_init(&_vbucket_comm_lock, NULL);

  // Initialize vbucket comm state
  for (int ii = 0; ii < _vbuckets; ++ii)
  {
    _vbucket_comm_state[ii] = OK;
  }

  if (config_reader != NULL)
  {
    // Create an updater to keep the store configured appropriately.
    _updater = new Updater<void, TopologyAwareMemcachedStore>(
                       this,
                       std::mem_fun(&TopologyAwareMemcachedStore::update_config));
  }
}


TopologyAwareMemcachedStore::~TopologyAwareMemcachedStore()
{
  // Clean up this thread's connection now, rather than waiting for
  // pthread_exit.  This is to support use by single-threaded code
  // (e.g., UTs), where pthread_exit is never called.
  connection* conn = (connection*)pthread_getspecific(_thread_local);
  if (conn != NULL)
  {
    pthread_setspecific(_thread_local, NULL);
    cleanup_connection(conn);
  }

  pthread_mutex_destroy(&_vbucket_comm_lock);

  pthread_rwlock_destroy(&_view_lock);

  pthread_key_delete(_thread_local);

  // Destroy the updater (if it was created) and the config reader.
  delete _updater; _updater = NULL;
  delete _config_reader; _config_reader = NULL;
}


/// Returns the vbucket for a specified key
int TopologyAwareMemcachedStore::vbucket_for_key(const std::string& key)
{
  // Hash the key and convert the hash to a vbucket.
  int hash = memcached_generate_hash_value(key.data(), key.length(), MEMCACHED_HASH_MD5);
  int vbucket = hash & (_vbuckets - 1);
  TRC_DEBUG("Key %s hashes to vbucket %d via hash 0x%x", key.c_str(), vbucket, hash);
  return vbucket;
}


/// Gets the set of replicas to use for a read or write operation for the
/// specified key.
const std::vector<memcached_st*>&
TopologyAwareMemcachedStore::get_replicas(const std::string& key, Op operation)
{
  return get_replicas(vbucket_for_key(key), operation);
}


/// Gets the set of replicas to use for a read or write operation for the
/// specified vbucket.
const std::vector<memcached_st*>& TopologyAwareMemcachedStore::get_replicas(int vbucket,
                                                                            Op operation)
{
  TopologyAwareMemcachedStore::connection* conn =
    (connection*)pthread_getspecific(_thread_local);

  if (conn == NULL)
  {
    // Create a new connection structure for this thread.
    conn = new TopologyAwareMemcachedStore::connection;
    pthread_setspecific(_thread_local, conn);
    conn->view_number = 0;
  }

  if (conn->view_number != _view_number)
  {
    // Either the view has changed or has not yet been set up, so set up the
    // connection and replica structures for this thread.
    for (std::map<std::string, memcached_st*>::iterator it = conn->st.begin();
         it != conn->st.end();
         it++)
    {
      memcached_free(it->second);
      it->second = NULL;
    }
    pthread_rwlock_rdlock(&_view_lock);

    TRC_DEBUG("Set up new view %d for thread", _view_number);

    // Create a set of memcached_st's one per server.
    for (size_t ii = 0; ii < _servers.size(); ++ii)
    {
      // Create a new memcached_st for this server.  Do not specify the server
      // at this point as memcached() does not support IPv6 addresses.
      TRC_DEBUG("Setting up server %d for connection %p (%s)", ii, conn, _options.c_str());
      conn->st[_servers[ii]] = memcached(_options.c_str(), _options.length());
      TRC_DEBUG("Set up connection %p to server %s", conn->st[_servers[ii]], _servers[ii].c_str());

      // Switch to a longer connect timeout from here on.
      memcached_behavior_set(conn->st[_servers[ii]], MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT, _max_connect_latency_ms);

      std::string server;
      int port;
      if (Utils::split_host_port(_servers[ii], server, port))
      {
        TRC_DEBUG("Setting server to IP address %s port %d",
                  server.c_str(),
                  port);
        memcached_server_add(conn->st[_servers[ii]], server.c_str(), port);
      }
      else
      {
        TRC_ERROR("Malformed host/port %s, skipping server", _servers[ii].c_str());
      }
    }

    conn->read_replicas.resize(_vbuckets);
    conn->write_replicas.resize(_vbuckets);

    // Now set up the read and write replica sets.
    for (int ii = 0; ii < _vbuckets; ++ii)
    {
      conn->read_replicas[ii].resize(_read_replicas[ii].size());
      for (size_t jj = 0; jj < _read_replicas[ii].size(); ++jj)
      {
        conn->read_replicas[ii][jj] = conn->st[_read_replicas[ii][jj]];
      }
      conn->write_replicas[ii].resize(_write_replicas[ii].size());
      for (size_t jj = 0; jj < _write_replicas[ii].size(); ++jj)
      {
        conn->write_replicas[ii][jj] = conn->st[_write_replicas[ii][jj]];
      }
    }

    // Flag that we are in sync with the latest view.
    conn->view_number = _view_number;

    pthread_rwlock_unlock(&_view_lock);
  }

  return (operation == Op::READ) ? conn->read_replicas[vbucket] : conn->write_replicas[vbucket];
}


/// Set up a new view of the memcached cluster(s).  The view determines
/// how data is distributed around the cluster.
void TopologyAwareMemcachedStore::new_view(const MemcachedConfig& config)
{
  TRC_STATUS("Updating memcached store configuration");

  // Create a new view with the new server lists.
  MemcachedStoreView view(_vbuckets, _replicas);
  view.update(config);

  // Now copy the view so it can be accessed by the worker threads.
  pthread_rwlock_wrlock(&_view_lock);

  // Get the list of servers from the view.
  _servers = view.servers();

  // For each vbucket, get the list of read replicas and write replicas.
  for (int ii = 0; ii < _vbuckets; ++ii)
  {
    _read_replicas[ii] = view.read_replicas(ii);
    _write_replicas[ii] = view.write_replicas(ii);
  }

  // Update the view number as the last thing here, otherwise we could stall
  // other threads waiting for the lock.
  TRC_STATUS("Finished preparing new view, so flag that workers should switch to it");
  ++_view_number;

  pthread_rwlock_unlock(&_view_lock);
}


void TopologyAwareMemcachedStore::update_config()
{
  MemcachedConfig cfg;

  if (_config_reader->read_config(cfg))
  {
    new_view(cfg);
  }
  else
  {
    TRC_ERROR("Failed to read config, keeping previous settings");
  }
}


/// Update state of vbucket replica communication. If alarms are configured, a set
/// alarm is issued if a vbucket becomes inaccessible, a clear alarm is issued once
/// all vbuckets become accessible again.
void TopologyAwareMemcachedStore::update_vbucket_comm_state(int vbucket, CommState state)
{
  if (_vbucket_alarm)
  {
    pthread_mutex_lock(&_vbucket_comm_lock);

    if (_vbucket_comm_state[vbucket] != state)
    {
      if (state == OK)
      {
        if (--_vbucket_comm_fail_count == 0)
        {
          _vbucket_alarm->clear();
        }

        TRC_INFO("vbucket %d now accessible, %d inaccessible vbucket(s) remain",
                 vbucket,
                 _vbucket_comm_fail_count);
      }
      else
      {
        _vbucket_comm_fail_count++;
        _vbucket_alarm->set();
        TRC_INFO("vbucket %d inaccessible, now %d inaccessible vbucket(s)",
                 vbucket,
                 _vbucket_comm_fail_count);
      }

      _vbucket_comm_state[vbucket] = state;
    }

    pthread_mutex_unlock(&_vbucket_comm_lock);
  }
}


/// Called to clean up the thread local data for a thread using the
/// TopologyAwareMemcachedStore class.
void TopologyAwareMemcachedStore::cleanup_connection(void* p)
{
  TopologyAwareMemcachedStore::connection* conn =
    (TopologyAwareMemcachedStore::connection*)p;

  for (std::map<std::string, memcached_st*>::iterator it = conn->st.begin();
       it != conn->st.end();
       it++)
  {
    memcached_free(it->second);
    it->second = NULL;
  }

  delete conn;
}


/// Retrieve the data for a given namespace and key.
Store::Status TopologyAwareMemcachedStore::get_data(const std::string& table,
                                                    const std::string& key,
                                                    std::string& data,
                                                    uint64_t& cas,
                                                    SAS::TrailId trail)
{
  Store::Status status;

  // Construct the fully qualified key.
  std::string fqkey = get_fq_key(table, key);
  const char* key_ptr = fqkey.data();
  const size_t key_len = fqkey.length();

  int vbucket = vbucket_for_key(fqkey);
  const std::vector<memcached_st*>& replicas = get_replicas(vbucket, Op::READ);

  if (trail != 0)
  {
    SAS::Event start(trail, SASEvent::MEMCACHED_GET_START, 0);
    start.add_var_param(fqkey);
    SAS::report_event(start);
  }

  TRC_DEBUG("%d read replicas for key %s", replicas.size(), fqkey.c_str());

  // Read from all replicas until we get a positive result.
  memcached_return_t rc = MEMCACHED_ERROR;
  bool active_not_found = false;
  size_t failed_replicas = 0;
  size_t ii;

  // If we only have one replica, we should try it twice -
  // libmemcached won't notice a dropped TCP connection until it tries
  // to make a request on it, and will fail the request then
  // reconnect, so the second attempt could still work.
  size_t attempts = (replicas.size() == 1) ? 2 : replicas.size();

  for (ii = 0; ii < attempts; ++ii)
  {
    size_t replica_idx;
    if ((replicas.size() == 1) && (ii == 1))
    {
      if (rc != MEMCACHED_CONNECTION_FAILURE)
      {
        // This is a legitimate error, not a server failure, so we
        // shouldn't retry.
        break;
      }
      replica_idx = 0;
      TRC_WARNING("Failed to read from sole memcached replica: retrying once");
    }
    else
    {
      replica_idx = ii;
    }

    TRC_DEBUG("Attempt to read from replica %d (connection %p)",
              replica_idx,
              replicas[replica_idx]);
    rc = get_from_replica(replicas[replica_idx], key_ptr, key_len, data, cas);

    if (memcached_success(rc))
    {
      // Got data back from this replica. Don't try any more.
      TRC_DEBUG("Read for %s on replica %d returned SUCCESS",
                fqkey.c_str(),
                replica_idx);
      break;
    }
    else if (rc == MEMCACHED_NOTFOUND)
    {
      // Failed to find a record on an active replica.  Flag this so if we do
      // find data on a later replica we can reset the cas value returned to
      // zero to ensure a subsequent write will succeed.
      TRC_DEBUG("Read for %s on replica %d returned NOTFOUND", fqkey.c_str(), replica_idx);
      active_not_found = true;
    }
    else
    {
      // Error from this node, so consider it inactive.
      TRC_DEBUG("Read for %s on replica %d returned error %d (%s)",
                fqkey.c_str(), replica_idx, rc, memcached_strerror(replicas[replica_idx], rc));
      ++failed_replicas;
    }
  }

  if (memcached_success(rc))
  {
    if (data != TOMBSTONE)
    {
      if (trail != 0)
      {
        SAS::Event got_data(trail, SASEvent::MEMCACHED_GET_SUCCESS, 0);
        got_data.add_var_param(fqkey);
        got_data.add_var_param(data);
        got_data.add_static_param(cas);
        SAS::report_event(got_data);
      }

      // Return the data and CAS value.  The CAS value is either set to the CAS
      // value from the result, or zero if an earlier active replica returned
      // NOT_FOUND.  This ensures that a subsequent set operation will succeed
      // on the earlier active replica.
      if (active_not_found)
      {
        cas = 0;
      }

      TRC_DEBUG("Read %d bytes from table %s key %s, CAS = %ld",
                data.length(), table.c_str(), key.c_str(), cas);
      status = Store::OK;
    }
    else
    {
      if (trail != 0)
      {
        SAS::Event got_tombstone(trail, SASEvent::MEMCACHED_GET_TOMBSTONE, 0);
        got_tombstone.add_var_param(fqkey);
        got_tombstone.add_static_param(cas);
        SAS::report_event(got_tombstone);
      }

      // We have read a tombstone. Return NOT_FOUND to the caller, and also
      // zero out the CAS (returning a zero CAS makes the interface cleaner).
      TRC_DEBUG("Read tombstone from table %s key %s, CAS = %ld",
                table.c_str(), key.c_str(), cas);
      cas = 0;
      status = Store::NOT_FOUND;
    }

    // Regardless of whether we got a tombstone, the vbucket is alive.
    update_vbucket_comm_state(vbucket, OK);

    if (_comm_monitor)
    {
      _comm_monitor->inform_success();
    }
  }
  else if (failed_replicas < replicas.size())
  {
    // At least one replica returned NOT_FOUND.
    if (trail != 0)
    {
      SAS::Event not_found(trail, SASEvent::MEMCACHED_GET_NOT_FOUND, 0);
      not_found.add_var_param(fqkey);
      SAS::report_event(not_found);
    }

    TRC_DEBUG("At least one replica returned not found, so return NOT_FOUND");
    status = Store::Status::NOT_FOUND;

    update_vbucket_comm_state(vbucket, OK);

    if (_comm_monitor)
    {
      _comm_monitor->inform_success();
    }
  }
  else
  {
    // All replicas returned an error, so log the error and return the
    // failure.
    if (trail != 0)
    {
      SAS::Event err(trail, SASEvent::MEMCACHED_GET_ERROR, 0);
      err.add_var_param(fqkey);
      err.add_var_param(memcached_strerror(NULL, rc));
      SAS::report_event(err);
    }

    TRC_ERROR("Failed to read data for %s from %d replicas with error %s",
              fqkey.c_str(), replicas.size(), memcached_strerror(NULL, rc));
    status = Store::Status::ERROR;

    update_vbucket_comm_state(vbucket, FAILED);

    if (_comm_monitor)
    {
      _comm_monitor->inform_failure();
    }
  }

  return status;
}


/// Update the data for the specified namespace and key.  Writes the data
/// atomically, so if the underlying data has changed since it was last
/// read, the update is rejected and this returns Store::Status::CONTENTION.
Store::Status TopologyAwareMemcachedStore::set_data(const std::string& table,
                                                    const std::string& key,
                                                    const std::string& data,
                                                    uint64_t cas,
                                                    int expiry,
                                                    SAS::TrailId trail)
{
  Store::Status status = Store::Status::ERROR;

  TRC_DEBUG("Writing %d bytes to table %s key %s, CAS = %ld, expiry = %d",
            data.length(), table.c_str(), key.c_str(), cas, expiry);

  // Construct the fully qualified key.
  std::string fqkey = get_fq_key(table, key);
  const char* key_ptr = fqkey.data();
  const size_t key_len = fqkey.length();

  int vbucket = vbucket_for_key(fqkey);
  const std::vector<memcached_st*>& replicas = get_replicas(vbucket, Op::WRITE);

  if (trail != 0)
  {
    SAS::Event start(trail, SASEvent::MEMCACHED_SET_START, 0);
    start.add_var_param(fqkey);
    start.add_var_param(data);
    start.add_static_param(cas);
    start.add_static_param(expiry);
    SAS::report_event(start);
  }

  TRC_DEBUG("%d write replicas for key %s", replicas.size(), fqkey.c_str());

  // Calculate a timestamp (least-significant 32 bits of milliseconds since the
  // epoch) for the current time.  We store this in the flags field to allow us
  // to resolve conflicts when resynchronizing between memcached servers.
  struct timespec ts;
  (void)clock_gettime(CLOCK_REALTIME, &ts);
  uint32_t flags = (uint32_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));

  // Memcached uses a flexible mechanism for specifying expiration.
  // - 0 indicates never expire.
  // - <= MEMCACHED_EXPIRATION_MAXDELTA indicates a relative (delta) time.
  // - > MEMCACHED_EXPIRATION_MAXDELTA indicates an absolute time.
  // Absolute time is the only way to force immediate expiry.  Unfortunately,
  // it's not reliable - see https://github.com/Metaswitch/cpp-common/issues/160
  // for details.  Instead, we use relative time for future times (expiry > 0)
  // and the earliest absolute time for immediate expiry (expiry == 0).
  time_t memcached_expiration =
    (time_t)((expiry > 0) ? expiry : MEMCACHED_EXPIRATION_MAXDELTA + 1);

  // First try to write the primary data record to the first responding
  // server.
  memcached_return_t rc = MEMCACHED_ERROR;
  size_t ii;
  size_t replica_idx;

  // If we only have one replica, we should try it twice -
  // libmemcached won't notice a dropped TCP connection until it tries
  // to make a request on it, and will fail the request then
  // reconnect, so the second attempt could still work.
  size_t attempts = (replicas.size() == 1) ? 2: replicas.size();

  for (ii = 0; ii < attempts; ++ii)
  {
    if ((replicas.size() == 1) && (ii == 1)) {
      if (rc != MEMCACHED_CONNECTION_FAILURE)
      {
        // This is a legitimate error, not a transient server failure, so we
        // shouldn't retry.
        break;
      }
      replica_idx = 0;
      TRC_WARNING("Failed to write to sole memcached replica: retrying once");
    }
    else
    {
      replica_idx = ii;
    }

    TRC_DEBUG("Attempt conditional write to vbucket %d on replica %d (connection %p), CAS = %ld, expiry = %d",
              vbucket,
              replica_idx,
              replicas[replica_idx],
              cas,
              expiry);

    if (cas == 0)
    {
      // New record, so attempt to add (but overwrite any tombstones we
      // encounter).  This will fail if someone else got there first and some
      // data already exists in memcached for this key.
      rc = add_overwriting_tombstone(replicas[replica_idx],
                                     key_ptr,
                                     key_len,
                                     vbucket,
                                     data,
                                     memcached_expiration,
                                     flags,
                                     trail);
    }
    else
    {
      // This is an update to an existing record, so use memcached_cas
      // to make sure it is atomic.
      rc = memcached_cas_vb(replicas[replica_idx],
                            key_ptr,
                            key_len,
                            _binary ? vbucket : 0,
                            data.data(),
                            data.length(),
                            memcached_expiration,
                            flags,
                            cas);

      if (!memcached_success(rc))
      {
        // Make a log,then move on to the next replica
        TRC_DEBUG("memcached_cas command failed, rc = %d (%s)\n%s",
                  rc,
                  memcached_strerror(replicas[replica_idx], rc),
                  memcached_last_error_message(replicas[replica_idx]));
      }
    }

    if (memcached_success(rc))
    {
      TRC_DEBUG("Conditional write succeeded to replica %d", replica_idx);
      status = Store::Status::OK;
      break;
    }
    else if ((rc == MEMCACHED_NOTFOUND) ||
             (rc == MEMCACHED_NOTSTORED) ||
             (rc == MEMCACHED_DATA_EXISTS))
    {

      // A NOT_STORED or EXISTS response indicates a concurrent write failure,
      // so return this to the application immediately - don't go on to
      // other replicas.
      TRC_INFO("Contention writing data for %s to store", fqkey.c_str());
      status = Store::Status::DATA_CONTENTION;
      break;
    }
  }

  if (status == Store::Status::OK)
  {
    // Write has succeeded, so write unconditionally (and asynchronously)
    // to the replicas.
    for (size_t jj = replica_idx + 1; jj < replicas.size(); ++jj)
    {
      TRC_DEBUG("Attempt unconditional write to replica %d", jj);
      memcached_behavior_set(replicas[jj], MEMCACHED_BEHAVIOR_NOREPLY, 1);
      memcached_set_vb(replicas[jj],
                      key_ptr,
                      key_len,
                      _binary ? vbucket : 0,
                      data.data(),
                      data.length(),
                      memcached_expiration,
                      flags);
      memcached_behavior_set(replicas[jj], MEMCACHED_BEHAVIOR_NOREPLY, 0);
    }

    update_vbucket_comm_state(vbucket, OK);

    if (_comm_monitor)
    {
      _comm_monitor->inform_success();
    }
  }
  else if (status == Store::Status::DATA_CONTENTION)
  {
    // Data contention is treated separately from other errors
    if (trail != 0)
    {
      SAS::Event err(trail, SASEvent::MEMCACHED_SET_CONTENTION, 0);
      err.add_var_param(fqkey);
      SAS::report_event(err);
    }

    // Commms are working if there was data contention
    update_vbucket_comm_state(vbucket, OK);

    if (_comm_monitor)
    {
      _comm_monitor->inform_success();
    }
  }
  else
  {
    // Generic error - log the error type to SAS
    if (trail != 0)
    {
      SAS::Event err(trail, SASEvent::MEMCACHED_SET_FAILED, 0);
      err.add_var_param(fqkey);
      err.add_var_param(memcached_strerror(NULL, rc));
      SAS::report_event(err);
    }

    update_vbucket_comm_state(vbucket, FAILED);

    if (_comm_monitor)
    {
      _comm_monitor->inform_failure();
    }

    TRC_ERROR("Failed to write data for %s to %d replicas with error %s",
              fqkey.c_str(), replicas.size(), memcached_strerror(NULL, rc));
  }

  return status;
}


/// Delete the data for the specified namespace and key.  Writes the data
/// unconditionally, so CAS is not needed.
Store::Status TopologyAwareMemcachedStore::delete_data(const std::string& table,
                                                       const std::string& key,
                                                       SAS::TrailId trail)
{
  TRC_DEBUG("Deleting key %s from table %s", key.c_str(), table.c_str());

  // Construct the fully qualified key.
  std::string fqkey = get_fq_key(table, key);

  // Delete from the read replicas - read replicas are a superset of the write replicas
  const std::vector<memcached_st*>& replicas = get_replicas(fqkey, Op::READ);
  TRC_DEBUG("Deleting from the %d read replicas for key %s", replicas.size(), fqkey.c_str());

  if (_tombstone_lifetime == 0)
  {
    delete_without_tombstone(fqkey, replicas, trail);
  }
  else
  {
    delete_with_tombstone(fqkey, replicas, trail);
  }

  return Status::OK;
}


void TopologyAwareMemcachedStore::delete_without_tombstone(const std::string& fqkey,
                                                           const std::vector<memcached_st*>& replicas,
                                                           SAS::TrailId trail)
{
  if (trail != 0)
  {
    SAS::Event event(trail, SASEvent::MEMCACHED_DELETE, 0);
    event.add_var_param(fqkey);
    SAS::report_event(event);
  }

  const char* key_ptr = fqkey.data();
  const size_t key_len = fqkey.length();

  for (size_t ii = 0; ii < replicas.size(); ++ii)
  {
    TRC_DEBUG("Attempt delete to replica %d (connection %p)",
              ii,
              replicas[ii]);

    memcached_return_t rc = memcached_delete(replicas[ii],
                                             key_ptr,
                                             key_len,
                                             0);

    if (!memcached_success(rc))
    {
      TRC_ERROR("Delete failed to replica %d", ii);
    }
  }
}


void TopologyAwareMemcachedStore::delete_with_tombstone(const std::string& fqkey,
                                                        const std::vector<memcached_st*>& replicas,
                                                        SAS::TrailId trail)
{
  if (trail != 0)
  {
    SAS::Event event(trail, SASEvent::MEMCACHED_DELETE, 0);
    event.add_var_param(fqkey);
    event.add_static_param(_tombstone_lifetime);
    SAS::report_event(event);
  }

  const char* key_ptr = fqkey.data();
  const size_t key_len = fqkey.length();

  // Calculate a timestamp (least-significant 32 bits of milliseconds since the
  // epoch) for the current time.  We store this in the flags field to allow us
  // to resolve conflicts when resynchronizing between memcached servers.
  struct timespec ts;
  (void)clock_gettime(CLOCK_REALTIME, &ts);
  uint32_t flags = (uint32_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));

  // Calculate the vbucket for this key.
  int vbucket = vbucket_for_key(fqkey);

  for (size_t ii = 0; ii < replicas.size(); ++ii)
  {
    TRC_DEBUG("Attempt write tombstone to replica %d (connection %p)",
              ii,
              replicas[ii]);

    memcached_return_t rc = memcached_set_vb(replicas[ii],
                                             key_ptr,
                                             key_len,
                                             _binary ? vbucket : 0,
                                             TOMBSTONE.data(),
                                             TOMBSTONE.length(),
                                             _tombstone_lifetime,
                                             flags);

    if (!memcached_success(rc))
    {
      TRC_ERROR("Delete failed to replica %d", ii);
    }
  }
}


//
// TopologyNeutralMemcachedStore methods
//

TopologyNeutralMemcachedStore::
TopologyNeutralMemcachedStore(const std::string& target_domain,
                              AstaireResolver* resolver,
                              bool remote_store,
                              BaseCommunicationMonitor* comm_monitor) :
  // Always use binary, as this is all Astaire supports.
  BaseMemcachedStore(true, comm_monitor, remote_store),
  _target_domain(target_domain),
  _resolver(resolver),
  _attempts(2),
  _conn_pool(60, _options)
{}

memcached_return_t TopologyNeutralMemcachedStore::iterate_through_targets(
    std::vector<AddrInfo>& targets,
    SAS::TrailId trail,
    std::function<memcached_return_t(ConnectionHandle<memcached_st*>&)> fn)
{
  memcached_return_t rc = MEMCACHED_SUCCESS;

  for (size_t ii = 0; ii < targets.size(); ++ii)
  {
    AddrInfo& target = targets[ii];

    TRC_DEBUG("Try server IP %s, port %d",
              target.address.to_string().c_str(),
              target.port);
    SAS::Event attempt(trail, SASEvent::MEMCACHED_TRY_HOST, 0);
    attempt.add_var_param(target.address.to_string());
    attempt.add_static_param(target.port);
    SAS::report_event(attempt);

    ConnectionHandle<memcached_st*> conn = _conn_pool.get_connection(target);

    // This is where we actually talk to memcached.
    rc = fn(conn);

    TRC_DEBUG("libmemcached returned %d", rc);

    if (memcached_success(rc))
    {
      // Success - nothing more to do.
      break;
    }
    else if (!can_retry_memcached_rc(rc))
    {
      // This return code means it is not worth retrying to another server.
      TRC_DEBUG("Return code means the request should not be retried");
      break;
    }
    else
    {
      // If we can retry to another memcached there is something wrong with this
      // particular target which means we should blacklist it.
      TRC_DEBUG("Blacklisting target");
      _resolver->blacklist(target);
    }
  }

  return rc;
}

Store::Status TopologyNeutralMemcachedStore::get_data(const std::string& table,
                                                      const std::string& key,
                                                      std::string& data,
                                                      uint64_t& cas,
                                                      SAS::TrailId trail)
{
  Store::Status status;
  std::vector<AddrInfo> targets;
  memcached_return_t rc;

  TRC_DEBUG("Start GET from table %s for key %s", table.c_str(), key.c_str());

  std::string fqkey = get_fq_key(table, key);

  if (trail != 0)
  {
    SAS::Event start(trail, SASEvent::MEMCACHED_GET_START, 0);
    start.add_var_param(fqkey);
    SAS::report_event(start);
  }

  if (!get_targets(targets, trail))
  {
    return ERROR;
  }

  // Do a GET to each target, stopping if we get a definitive success/failure
  // response.
  //
  // The code that does the GET operation is passed as a lambda that captures
  // all necessary variables by reference.
  rc = iterate_through_targets(targets, trail,
                               [&](ConnectionHandle<memcached_st*>& conn_handle) {
     return get_from_replica(conn_handle.get_connection(),
                             fqkey.data(),
                             fqkey.length(),
                             data,
                             cas);
  });

  if (memcached_success(rc))
  {
    if (data != TOMBSTONE)
    {
      if (trail != 0)
      {
        SAS::Event got_data(trail, SASEvent::MEMCACHED_GET_SUCCESS, 0);
        got_data.add_var_param(fqkey);
        got_data.add_var_param(data);
        got_data.add_static_param(cas);
        SAS::report_event(got_data);
      }

      TRC_DEBUG("Read %d bytes from table %s key %s, CAS = %ld",
                data.length(), table.c_str(), key.c_str(), cas);
      status = Store::OK;
    }
    else
    {
      if (trail != 0)
      {
        SAS::Event got_tombstone(trail, SASEvent::MEMCACHED_GET_TOMBSTONE, 0);
        got_tombstone.add_var_param(fqkey);
        got_tombstone.add_static_param(cas);
        SAS::report_event(got_tombstone);
      }

      // We have read a tombstone. Return NOT_FOUND to the caller, and also
      // zero out the CAS (returning a zero CAS makes the interface cleaner).
      TRC_DEBUG("Read tombstone from table %s key %s, CAS = %ld",
                table.c_str(), key.c_str(), cas);
      cas = 0;
      status = Store::NOT_FOUND;
    }

    if (_comm_monitor)
    {
      _comm_monitor->inform_success();
    }
  }
  else if (rc == MEMCACHED_NOTFOUND)
  {
    TRC_DEBUG("Key not found");

    if (trail != 0)
    {
      SAS::Event not_found(trail, SASEvent::MEMCACHED_GET_NOT_FOUND, 0);
      not_found.add_var_param(fqkey);
      SAS::report_event(not_found);
    }

    status = Store::Status::NOT_FOUND;

    if (_comm_monitor)
    {
      _comm_monitor->inform_success();
    }
  }
  else
  {
    if (trail != 0)
    {
      SAS::Event err(trail, SASEvent::MEMCACHED_GET_ERROR, 0);
      err.add_var_param(fqkey);
      err.add_var_param(memcached_strerror(NULL, rc));
      SAS::report_event(err);
    }

    TRC_DEBUG("Failed to read data with error %s", memcached_strerror(NULL, rc));
    status = Store::Status::ERROR;

    if (_comm_monitor)
    {
      _comm_monitor->inform_failure();
    }
  }

  return status;
}


Store::Status TopologyNeutralMemcachedStore::set_data(const std::string& table,
                                                      const std::string& key,
                                                      const std::string& data,
                                                      uint64_t cas,
                                                      int expiry,
                                                      SAS::TrailId trail)
{
  Store::Status status = Store::Status::OK;
  std::vector<AddrInfo> targets;
  memcached_return_t rc;

  TRC_DEBUG("Writing %d bytes to table %s key %s, CAS = %ld, expiry = %d",
            data.length(), table.c_str(), key.c_str(), cas, expiry);

  std::string fqkey = get_fq_key(table, key);

  if (trail != 0)
  {
    SAS::Event start(trail, SASEvent::MEMCACHED_SET_START, 0);
    start.add_var_param(fqkey);
    start.add_var_param(data);
    start.add_static_param(cas);
    start.add_static_param(expiry);
    SAS::report_event(start);
  }

  // Memcached uses a flexible mechanism for specifying expiration.
  // - 0 indicates never expire.
  // - <= MEMCACHED_EXPIRATION_MAXDELTA indicates a relative (delta) time.
  // - > MEMCACHED_EXPIRATION_MAXDELTA indicates an absolute time.
  // Absolute time is the only way to force immediate expiry.  Unfortunately,
  // it's not reliable - see https://github.com/Metaswitch/cpp-common/issues/160
  // for details.  Instead, we use relative time for future times (expiry > 0)
  // and the earliest absolute time for immediate expiry (expiry == 0).
  time_t memcached_expiration =
    (time_t)((expiry > 0) ? expiry : MEMCACHED_EXPIRATION_MAXDELTA + 1);

  if (!get_targets(targets, trail))
  {
    return ERROR;
  }

  // Do a ADD/CAS to each replica (depending on the cas value), stopping if we
  // get a definitive success/failure response.
  //
  // The code that does the operation is passed as a lambda that captures all
  // necessary variables by reference.
  rc = iterate_through_targets(targets, trail,
                               [&](ConnectionHandle<memcached_st*>& conn_handle) {
    memcached_return_t rc;

    if (cas == 0)
    {
      // New record, so attempt to add (but overwrite any tombstones we
      // encounter).  This will fail if someone else got there first and some
      // data already exists in memcached for this key.
      rc = add_overwriting_tombstone(conn_handle.get_connection(),
                                     fqkey.data(),
                                     fqkey.length(),
                                     0,
                                     data,
                                     memcached_expiration,
                                     0,
                                     trail);
    }
    else
    {
      // This is an update to an existing record, so use memcached_cas
      // to make sure it is atomic.
      rc = memcached_cas_vb(conn_handle.get_connection(),
                            fqkey.data(),
                            fqkey.length(),
                            0,
                            data.data(),
                            data.length(),
                            memcached_expiration,
                            0,
                            cas);
    }
    return rc;
  });

  if (memcached_success(rc))
  {
    if (_comm_monitor)
    {
      _comm_monitor->inform_success();
    }

    TRC_DEBUG("Write successful");
    status = Store::OK;
  }
  else if ((rc == MEMCACHED_NOTFOUND) ||
           (rc == MEMCACHED_NOTSTORED) ||
           (rc == MEMCACHED_DATA_EXISTS))
  {
    if (trail != 0)
    {
      SAS::Event err(trail, SASEvent::MEMCACHED_SET_CONTENTION, 0);
      err.add_var_param(fqkey);
      SAS::report_event(err);
    }

    if (_comm_monitor)
    {
      _comm_monitor->inform_success();
    }

    TRC_DEBUG("Contention writing data for %s to store", fqkey.c_str());
    status = Store::Status::DATA_CONTENTION;
  }
  else
  {
    if (trail != 0)
    {
      SAS::Event err(trail, SASEvent::MEMCACHED_SET_FAILED, 0);
      err.add_var_param(fqkey);
      err.add_var_param(memcached_strerror(NULL, rc));
      SAS::report_event(err);
    }

    if (_comm_monitor)
    {
      _comm_monitor->inform_failure();
    }

    TRC_DEBUG("Failed to write data for %s to store with error %s",
              fqkey.c_str(), memcached_strerror(NULL, rc));
    status = Store::Status::ERROR;
  }

  return status;
}


Store::Status TopologyNeutralMemcachedStore::delete_data(const std::string& table,
                                                         const std::string& key,
                                                         SAS::TrailId trail)
{
  Store::Status status = ERROR;
  std::vector<AddrInfo> targets;
  memcached_return_t rc;

  TRC_DEBUG("Deleting key %s from table %s", key.c_str(), table.c_str());

  std::string fqkey = get_fq_key(table, key);

  if (_tombstone_lifetime == 0)
  {
    SAS::Event event(trail, SASEvent::MEMCACHED_DELETE, 0);
    event.add_var_param(fqkey);
    SAS::report_event(event);
  }
  else
  {
    SAS::Event event(trail, SASEvent::MEMCACHED_DELETE, 0);
    event.add_var_param(fqkey);
    event.add_static_param(_tombstone_lifetime);
    SAS::report_event(event);
  }

  if (!get_targets(targets, trail))
  {
    return ERROR;
  }

  // Do a DELETE/SET to each target (depending on whether we should we writing
  // tombstones or not), stopping if we get a definitive success/failure
  // response.
  //
  // The code that does the operation is passed as a lambda that captures all
  // necessary variables by reference.
  rc = iterate_through_targets(targets, trail,
                               [&](ConnectionHandle<memcached_st*>& conn_handle) {
    memcached_return_t rc;

    if (_tombstone_lifetime == 0)
    {
      rc = memcached_delete(conn_handle.get_connection(), fqkey.data(), fqkey.length(), 0);
    }
    else
    {
      rc = memcached_set_vb(conn_handle.get_connection(),
                            fqkey.data(),
                            fqkey.length(),
                            0,
                            TOMBSTONE.data(),
                            TOMBSTONE.length(),
                            _tombstone_lifetime,
                            0);
    }
    return rc;
  });

  if (memcached_success(rc))
  {
    status = OK;
  }
  else
  {
    if (trail != 0)
    {
      SAS::Event event(trail, SASEvent::MEMCACHED_DELETE_FAILURE, 0);
      event.add_var_param(fqkey);
      event.add_var_param(memcached_strerror(NULL, rc));
      SAS::report_event(event);
    }

    TRC_DEBUG("Delete failed with error %s", memcached_strerror(NULL, rc));
  }

  return status;
}


bool TopologyNeutralMemcachedStore::can_retry_memcached_rc(memcached_return_t rc)
{
  return (!memcached_success(rc) &&
          (rc != MEMCACHED_NOTFOUND) &&
          (rc != MEMCACHED_NOTSTORED) &&
          (rc != MEMCACHED_DATA_EXISTS) &&
          (rc != MEMCACHED_E2BIG));
}


bool TopologyNeutralMemcachedStore::get_targets(std::vector<AddrInfo>& targets,
                                                SAS::TrailId trail)
{
  // Resolve the Astaire domain into a list of potential targets.
  _resolver->resolve(_target_domain, _attempts, targets, trail);
  TRC_DEBUG("Found %d targets for %s", targets.size(), _target_domain.c_str());

  if (targets.empty())
  {
    TRC_DEBUG("No targets in domain - give up");
    SAS::Event event(trail, SASEvent::MEMCACHED_NO_HOSTS, 0);
    event.add_var_param(_target_domain);
    SAS::report_event(event);

    if (_comm_monitor != NULL)
    {
      _comm_monitor->inform_failure();
    }

    return false;
  }

  // Always try at least twice even if there is only one target.  This is
  // because if we have a connection for which the server is no longer working,
  // libmemcached will fail the request and only fixes the connection up
  // asynchronously.
  if (targets.size() == 1)
  {
    TRC_DEBUG("Duplicate target IP=%s, port= %d as it is the only target",
              targets[0].address.to_string().c_str(),
              targets[0].port);
    targets.push_back(targets[0]);
  }

  return true;
}
// LCOV_EXCL_STOP
