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


BaseMemcachedStore::BaseMemcachedStore(bool binary,
                                       MemcachedConfigReader* config_reader,
                                       CommunicationMonitor* comm_monitor,
                                       Alarm* vbucket_alarm) :
  _updater(NULL),
  _replicas(2),
  _vbuckets(128),
  _options(),
  _view_number(0),
  _servers(),
  _max_connect_latency_ms(50),
  _read_replicas(_vbuckets),
  _write_replicas(_vbuckets),
  _comm_monitor(comm_monitor),
  _vbucket_comm_state(_vbuckets),
  _vbucket_comm_fail_count(0),
  _vbucket_alarm(vbucket_alarm),
  _tombstone_lifetime(0),
  _config_reader(config_reader)
{
  // Create the thread local key for the per thread data.
  pthread_key_create(&_thread_local, BaseMemcachedStore::cleanup_connection);

  // Create the lock for protecting the current view.
  pthread_rwlock_init(&_view_lock, NULL);

  // Create the mutex for protecting vbucket comm state.
  pthread_mutex_init(&_vbucket_comm_lock, NULL);

  // Set up the fixed options for memcached.  We use a very short connect
  // timeout because libmemcached tries to connect to all servers sequentially
  // during start-up, and if any are not up we don't want to wait for any
  // significant length of time.
  _options = "--CONNECT-TIMEOUT=10 --SUPPORT-CAS --POLL-TIMEOUT=250";
  _options += (binary) ? " --BINARY_PROTOCOL" : "";

  // Create an updater to keep the store configured appropriately.
  _updater = new Updater<void, BaseMemcachedStore>(this, std::mem_fun(&BaseMemcachedStore::update_config));

  // Initialize vbucket comm state
  for (int ii = 0; ii < _vbuckets; ++ii)
  {
    _vbucket_comm_state[ii] = OK;
  }
}


BaseMemcachedStore::~BaseMemcachedStore()
{
  // Destroy the updater (if it was created) and the config reader.
  delete _updater; _updater = NULL;
  delete _config_reader; _config_reader = NULL;

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
}


// LCOV_EXCL_START - need real memcached to test
void BaseMemcachedStore::set_max_connect_latency(unsigned int ms)
{
  _max_connect_latency_ms = ms;
}

/// Set up a new view of the memcached cluster(s).  The view determines
/// how data is distributed around the cluster.
void BaseMemcachedStore::new_view(const MemcachedConfig& config)
{
  LOG_STATUS("Updating memcached store configuration");

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
  LOG_STATUS("Finished preparing new view, so flag that workers should switch to it");
  ++_view_number;

  pthread_rwlock_unlock(&_view_lock);
}


void BaseMemcachedStore::update_config()
{
  MemcachedConfig cfg;

  if (_config_reader->read_config(cfg))
  {
    new_view(cfg);
    _tombstone_lifetime = cfg.tombstone_lifetime;
  }
  else
  {
    LOG_ERROR("Failed to read config, keeping previous settings");
  }
}


/// Returns the vbucket for a specified key
int BaseMemcachedStore::vbucket_for_key(const std::string& key)
{
  // Hash the key and convert the hash to a vbucket.
  int hash = memcached_generate_hash_value(key.data(), key.length(), MEMCACHED_HASH_MD5);
  int vbucket = hash & (_vbuckets - 1);
  LOG_DEBUG("Key %s hashes to vbucket %d via hash 0x%x", key.c_str(), vbucket, hash);
  return vbucket;
}


/// Gets the set of replicas to use for a read or write operation for the
/// specified key.
const std::vector<memcached_st*>&
BaseMemcachedStore::get_replicas(const std::string& key, Op operation)
{
  return get_replicas(vbucket_for_key(key), operation);
}


/// Gets the set of replicas to use for a read or write operation for the
/// specified vbucket.
const std::vector<memcached_st*>& BaseMemcachedStore::get_replicas(int vbucket,
                                                                   Op operation)
{
  BaseMemcachedStore::connection* conn = (connection*)pthread_getspecific(_thread_local);
  if (conn == NULL)
  {
    // Create a new connection structure for this thread.
    conn = new BaseMemcachedStore::connection;
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

    LOG_DEBUG("Set up new view %d for thread", _view_number);

    // Create a set of memcached_st's one per server.
    for (size_t ii = 0; ii < _servers.size(); ++ii)
    {
      // Create a new memcached_st for this server.  Do not specify the server
      // at this point as memcached() does not support IPv6 addresses.
      LOG_DEBUG("Setting up server %d for connection %p (%s)", ii, conn, _options.c_str());
      conn->st[_servers[ii]] = memcached(_options.c_str(), _options.length());
      LOG_DEBUG("Set up connection %p to server %s", conn->st[_servers[ii]], _servers[ii].c_str());

      // Switch to a longer connect timeout from here on.
      memcached_behavior_set(conn->st[_servers[ii]], MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT, _max_connect_latency_ms);

      // Connect to the server.  The address is specified as either <IPv4 address>:<port>
      // or [<IPv6 address>]:<port>.  Look for square brackets to determine whether
      // this is an IPv6 address.
      std::vector<std::string> contact_details;
      size_t close_bracket = _servers[ii].find(']');

      if (close_bracket == _servers[ii].npos)
      {
        // IPv4 connection.  Split the string on the colon.
        Utils::split_string(_servers[ii], ':', contact_details);
        if (contact_details.size() != 2)
        {
          LOG_ERROR("Malformed contact details %s", _servers[ii].c_str());
          break;
        }
      }
      else
      {
        // IPv6 connection.  Split the string on ']', which removes any white
        // space from the start and the end, then remove the '[' from the
        // start of the IP addreess string and the start of the ';' from the start
        // of the port string.
        Utils::split_string(_servers[ii], ']', contact_details);
        if ((contact_details.size() != 2) ||
            (contact_details[0][0] != '[') ||
            (contact_details[1][0] != ':'))
        {
          LOG_ERROR("Malformed contact details %s", _servers[ii].c_str());
          break;
        }

        contact_details[0].erase(contact_details[0].begin());
        contact_details[1].erase(contact_details[1].begin());
      }

      LOG_DEBUG("Setting server to IP address %s port %s",
                contact_details[0].c_str(),
                contact_details[1].c_str());
      int port = atoi(contact_details[1].c_str());
      memcached_server_add(conn->st[_servers[ii]], contact_details[0].c_str(), port);
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


/// Update state of vbucket replica communication. If alarms are configured, a set
/// alarm is issued if a vbucket becomes inaccessible, a clear alarm is issued once
/// all vbuckets become accessible again.
void BaseMemcachedStore::update_vbucket_comm_state(int vbucket, CommState state)
{
  if (_vbucket_alarm)
  {
    pthread_mutex_lock(&_vbucket_comm_lock);

    if (_vbucket_comm_state[vbucket] != state)
    {
      if (state == OK)
      {
        if ((_vbucket_comm_fail_count--) == 0)
        {
          _vbucket_alarm->clear();
        }
      }
      else
      {
        _vbucket_comm_fail_count++;
        _vbucket_alarm->set();
      }

      _vbucket_comm_state[vbucket] = state;
    }

    pthread_mutex_unlock(&_vbucket_comm_lock);
  }
}


/// Called to clean up the thread local data for a thread using the
/// BaseMemcachedStore class.
void BaseMemcachedStore::cleanup_connection(void* p)
{
  BaseMemcachedStore::connection* conn = (BaseMemcachedStore::connection*)p;

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
Store::Status BaseMemcachedStore::get_data(const std::string& table,
                                           const std::string& key,
                                           std::string& data,
                                           uint64_t& cas,
                                           SAS::TrailId trail)
{
  Store::Status status;

  // Construct the fully qualified key.
  std::string fqkey = table + "\\\\" + key;
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

  LOG_DEBUG("%d read replicas for key %s", replicas.size(), fqkey.c_str());

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
      LOG_WARNING("Failed to read from sole memcached replica: retrying once");
    }
    else
    {
      replica_idx = ii;
    }

    LOG_DEBUG("Attempt to read from replica %d (connection %p)",
              replica_idx,
              replicas[replica_idx]);
    rc = get_from_replica(replicas[replica_idx], key_ptr, key_len, data, cas);

    if (memcached_success(rc))
    {
      // Got data back from this replica. Don't try any more.
      LOG_DEBUG("Read for %s on replica %d returned SUCCESS",
                fqkey.c_str(),
                replica_idx);
      break;
    }
    else if (rc == MEMCACHED_NOTFOUND)
    {
      // Failed to find a record on an active replica.  Flag this so if we do
      // find data on a later replica we can reset the cas value returned to
      // zero to ensure a subsequent write will succeed.
      LOG_DEBUG("Read for %s on replica %d returned NOTFOUND", fqkey.c_str(), replica_idx);
      active_not_found = true;
    }
    else
    {
      // Error from this node, so consider it inactive.
      LOG_DEBUG("Read for %s on replica %d returned error %d (%s)",
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

      LOG_DEBUG("Read %d bytes from table %s key %s, CAS = %ld",
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
      LOG_DEBUG("Read tombstone from table %s key %s, CAS = %ld",
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

    LOG_DEBUG("At least one replica returned not found, so return NOT_FOUND");
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
      SAS::report_event(err);
    }

    LOG_ERROR("Failed to read data for %s from %d replicas",
              fqkey.c_str(), replicas.size());
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
Store::Status BaseMemcachedStore::set_data(const std::string& table,
                                           const std::string& key,
                                           const std::string& data,
                                           uint64_t cas,
                                           int expiry,
                                           SAS::TrailId trail)
{
  Store::Status status = Store::Status::OK;

  LOG_DEBUG("Writing %d bytes to table %s key %s, CAS = %ld, expiry = %d",
            data.length(), table.c_str(), key.c_str(), cas, expiry);

  // Construct the fully qualified key.
  std::string fqkey = table + "\\\\" + key;
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

  LOG_DEBUG("%d write replicas for key %s", replicas.size(), fqkey.c_str());

  // Calculate the rough expected expiry time.  We store this in the flags
  // as it may be useful in future for read repair function.
  uint32_t now = time(NULL);
  uint32_t exptime = now + expiry;

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
      LOG_WARNING("Failed to write to sole memcached replica: retrying once");
    }
    else
    {
      replica_idx = ii;
    }

    LOG_DEBUG("Attempt conditional write to replica %d (connection %p), CAS = %ld, expiry = %d",
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
                                     data,
                                     memcached_expiration,
                                     exptime,
                                     trail);
    }
    else
    {
      // This is an update to an existing record, so use memcached_cas
      // to make sure it is atomic.
      rc = memcached_cas(replicas[replica_idx],
                         key_ptr,
                         key_len,
                         data.data(),
                         data.length(),
                         memcached_expiration,
                         exptime,
                         cas);

      if (!memcached_success(rc))
      {
        LOG_DEBUG("memcached_cas command failed, rc = %d (%s)\n%s",
                  rc,
                  memcached_strerror(replicas[replica_idx], rc),
                  memcached_last_error_message(replicas[replica_idx]));
      }
    }

    if (memcached_success(rc))
    {
      LOG_DEBUG("Conditional write succeeded to replica %d", replica_idx);
      break;
    }
    else if ((rc == MEMCACHED_NOTSTORED) ||
             (rc == MEMCACHED_DATA_EXISTS))
    {
      if (trail != 0)
      {
        SAS::Event err(trail, SASEvent::MEMCACHED_SET_CONTENTION, 0);
        err.add_var_param(fqkey);
        SAS::report_event(err);
      }

      // A NOT_STORED or EXISTS response indicates a concurrent write failure,
      // so return this to the application immediately - don't go on to
      // other replicas.
      LOG_INFO("Contention writing data for %s to store", fqkey.c_str());
      status = Store::Status::DATA_CONTENTION;
      break;
    }
  }

  if ((rc == MEMCACHED_SUCCESS) &&
      (replica_idx < replicas.size()))
  {
    // Write has succeeded, so write unconditionally (and asynchronously)
    // to the replicas.
    for (size_t jj = replica_idx + 1; jj < replicas.size(); ++jj)
    {
      LOG_DEBUG("Attempt unconditional write to replica %d", jj);
      memcached_behavior_set(replicas[jj], MEMCACHED_BEHAVIOR_NOREPLY, 1);
      memcached_set(replicas[jj],
                    key_ptr,
                    key_len,
                    data.data(),
                    data.length(),
                    memcached_expiration,
                    exptime);
      memcached_behavior_set(replicas[jj], MEMCACHED_BEHAVIOR_NOREPLY, 0);
    }
  }

  if ((!memcached_success(rc)) &&
      (rc != MEMCACHED_NOTSTORED) &&
      (rc != MEMCACHED_DATA_EXISTS))
  {
    if (trail != 0)
    {
      SAS::Event err(trail, SASEvent::MEMCACHED_SET_FAILED, 0);
      err.add_var_param(fqkey);
      SAS::report_event(err);
    }

    update_vbucket_comm_state(vbucket, FAILED);

    if (_comm_monitor)
    {
      _comm_monitor->inform_failure();
    }

    LOG_ERROR("Failed to write data for %s to %d replicas",
              fqkey.c_str(), replicas.size());
    status = Store::Status::ERROR;
  }
  else
  {
    update_vbucket_comm_state(vbucket, OK);

    if (_comm_monitor)
    {
      _comm_monitor->inform_success();
    }
  }

  return status;
}


/// Delete the data for the specified namespace and key.  Writes the data
/// unconditionally, so CAS is not needed.
Store::Status BaseMemcachedStore::delete_data(const std::string& table,
                                              const std::string& key,
                                              SAS::TrailId trail)
{
  LOG_DEBUG("Deleting key %s from table %s", key.c_str(), table.c_str());

  // Construct the fully qualified key.
  std::string fqkey = table + "\\\\" + key;

  // Delete from the read replicas - read replicas are a superset of the write replicas
  const std::vector<memcached_st*>& replicas = get_replicas(fqkey, Op::READ);
  LOG_DEBUG("Deleting from the %d read replicas for key %s", replicas.size(), fqkey.c_str());

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
    LOG_DEBUG("Fetch result");
    memcached_result_st result;
    memcached_result_create(replica, &result);
    memcached_fetch_result(replica, &result, &rc);

    if (memcached_success(rc))
    {
      // Found a record, so exit the read loop.
      LOG_DEBUG("Found record on replica");

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
                                                                 const std::string& data,
                                                                 time_t memcached_expiration,
                                                                 uint32_t flags,
                                                                 SAS::TrailId trail)
{
  memcached_return_t rc;
  uint64_t cas = 0;

  LOG_DEBUG("Attempting to add data for key %.*s", key_len, key_ptr);

  // Convert the key into a std::string (sas-client does not like that
  // key_{ptr,len} are constant).
  const std::string key(key_ptr, key_len);

  while (true)
  {
    if (cas == 0)
    {
      LOG_DEBUG("Attempting memcached ADD command");
      rc = memcached_add(replica,
                         key_ptr,
                         key_len,
                         data.data(),
                         data.length(),
                         memcached_expiration,
                         flags);
    }
    else
    {
      LOG_DEBUG("Attempting memcached CAS command (cas = %d)", cas);
      rc = memcached_cas(replica,
                         key_ptr,
                         key_len,
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

      LOG_DEBUG("Existing data prevented the ADD/CAS."
                "Issue GET to see if we need to overwrite a tombstone");
      get_rc = get_from_replica(replica, key_ptr, key_len, existing_data, cas);

      if (memcached_success(get_rc))
      {
        if (existing_data != TOMBSTONE)
        {
          // The existing record is not a tombstone.  We mustn't overwrite
          // this, so break out of the loop and return the original return code
          // from the ADD/CAS.
          LOG_DEBUG("Found real data. Give up");
          break;
        }
        else
        {
          // The existing record IS a tombstone. Go round the loop again to
          // overwrite it. `cas` has been set to the cas of the tombstone.
          LOG_DEBUG("Found a tombstone. Attempt to overwrite");

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
        LOG_DEBUG("GET failed with NOT_FOUND");

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
        LOG_DEBUG("GET failed, rc = %d (%s)\n%s",
                  get_rc,
                  memcached_strerror(replica, get_rc),
                  memcached_last_error_message(replica));
        break;
      }
    }
    else
    {
      LOG_DEBUG("ADD/CAS returned rc = %d (%s)\n%s",
                rc,
                memcached_strerror(replica, rc),
                memcached_last_error_message(replica));
      break;
    }
  }

  return rc;
}


void BaseMemcachedStore::delete_without_tombstone(const std::string& fqkey,
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
    LOG_DEBUG("Attempt delete to replica %d (connection %p)",
              ii,
              replicas[ii]);

    memcached_return_t rc = memcached_delete(replicas[ii],
                                             key_ptr,
                                             key_len,
                                             0);

    if (!memcached_success(rc))
    {
      log_delete_failure(fqkey, ii, replicas.size(), trail, 0);
    }
  }
}


void BaseMemcachedStore::delete_with_tombstone(const std::string& fqkey,
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

  // Calculate the rough expected expiry time.  We store this in the flags
  // as it may be useful in future for read repair function.
  uint32_t now = time(NULL);
  uint32_t exptime = now + _tombstone_lifetime;

  for (size_t ii = 0; ii < replicas.size(); ++ii)
  {
    LOG_DEBUG("Attempt write tombstone to replica %d (connection %p)",
              ii,
              replicas[ii]);

    memcached_return_t rc = memcached_set(replicas[ii],
                                          key_ptr,
                                          key_len,
                                          TOMBSTONE.data(),
                                          TOMBSTONE.length(),
                                          _tombstone_lifetime,
                                          exptime);

    if (!memcached_success(rc))
    {
      log_delete_failure(fqkey, ii, replicas.size(), trail, 1);
    }
  }
}

void BaseMemcachedStore::log_delete_failure(const std::string& fqkey,
                                            int replica_ix,
                                            int replica_count,
                                            SAS::TrailId trail,
                                            uint32_t instance)
{
  LOG_ERROR("Delete failed to replica %d", replica_ix);

  if (trail != 0)
  {
    SAS::Event event(trail, SASEvent::MEMCACHED_DELETE_FAILURE, instance);
    event.add_var_param(fqkey);
    event.add_static_param(replica_ix);
    event.add_static_param(replica_count);
    SAS::report_event(event);
  }
}


MemcachedStore::MemcachedStore(bool binary,
                               const std::string& config_file,
                               CommunicationMonitor* comm_monitor,
                               Alarm* vbucket_alarm) :
  BaseMemcachedStore(binary,
                     new MemcachedConfigFileReader(config_file),
                     comm_monitor,
                     vbucket_alarm)
{}

MemcachedStore::MemcachedStore(bool binary,
                               MemcachedConfigReader* config_reader,
                               CommunicationMonitor* comm_monitor,
                               Alarm* vbucket_alarm) :
  BaseMemcachedStore(binary,
                     config_reader,
                     comm_monitor,
                     vbucket_alarm)
{}

// LCOV_EXCL_STOP
