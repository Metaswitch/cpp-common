/**
 * @file memcachedstore.cpp Memcached-backed implementation of the registration data store.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
                                       bool remote_store,
                                       BaseCommunicationMonitor* comm_monitor) :
  _binary(binary),
  _options(),
  _comm_monitor(comm_monitor),
  _tombstone_lifetime(200)
{
  // Set up the fixed options for memcached.  See also the options configured
  // on the MemcachedConnectionPool (including the connect timeout).
  _options = "--SUPPORT-CAS";

  // When the MemcachedStore is being used to write to memcached via Rogers,
  // the poll-timeout needs to be long enough to accomodate Rogers failing to
  // connect and/or access a failed memcached replica which can take a long
  // time: see comment on the values of connection latency used in the
  // MemcachedConnectionPool.
  //
  // - For a local store, we need to allow sufficient time for Rogers to fail
  //   to connect and access one replica and then succeed in accessing another.
  // - For a remote store, we need to allow the same time + 100ms latency in
  //   each direction.
  _options += (remote_store) ? " --POLL-TIMEOUT=300" : " --POLL-TIMEOUT=100";
  _options += (_binary) ? " --BINARY-PROTOCOL" : "";

  TRC_DEBUG("Memcached options: %s", _options.c_str());
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
  CW_IO_STARTS("Memcached GET for " + std::string(key_ptr, key_len))
  {
    rc = memcached_mget(replica, &key_ptr, &key_len, 1);
  }
  CW_IO_COMPLETES()

  if (memcached_success(rc))
  {
    // memcached_mget command was successful, so retrieve the result.
    TRC_DEBUG("Fetch result");
    memcached_result_st result;
    memcached_result_create(replica, &result);

    CW_IO_STARTS("Memcached GET fetch result for " + std::string(key_ptr, key_len))
    {
      memcached_fetch_result(replica, &result, &rc);
    }
    CW_IO_COMPLETES()

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
      CW_IO_STARTS("Memcached ADD for " + std::string(key_ptr, key_len))
      {
        rc = memcached_add_vb(replica,
                              key_ptr,
                              key_len,
                              _binary ? vbucket : 0,
                              data.data(),
                              data.length(),
                              memcached_expiration,
                              flags);
      }
      CW_IO_COMPLETES()
    }
    else
    {
      TRC_DEBUG("Attempting memcached CAS command (cas = %d)", cas);
      CW_IO_STARTS("Memcached CAS for " + std::string(key_ptr, key_len))
      {
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
      CW_IO_COMPLETES()
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
// TopologyNeutralMemcachedStore methods
//

TopologyNeutralMemcachedStore::
TopologyNeutralMemcachedStore(const std::string& target_domain,
                              AstaireResolver* resolver,
                              bool remote_store,
                              BaseCommunicationMonitor* comm_monitor) :
  // Always use binary, as this is all Astaire supports.
  BaseMemcachedStore(true, remote_store, comm_monitor),
  _target_domain(target_domain),
  _resolver(resolver),
  _attempts(2),
  _conn_pool(60, _options, remote_store)
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
                                                      SAS::TrailId trail,
                                                      bool log_body,
                                                      Format data_format)
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
        int event;

        if (log_body)
        {
          event = SASEvent::MEMCACHED_GET_SUCCESS;
        }
        else
        {
          event = SASEvent::MEMCACHED_GET_WITHOUT_DATA_SUCCESS;
        }

        SAS::Event got_data(trail, event, 0);
        got_data.add_var_param(fqkey);
        got_data.add_static_param(cas);

        if (log_body)
        {
          got_data.add_var_param(data);
          got_data.add_static_param(data_format);
        }
     
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
                                                      SAS::TrailId trail,
                                                      bool log_body,
                                                      Store::Format data_format)
{
  TRC_DEBUG("Writing %d bytes to table %s key %s, CAS = %ld, expiry = %d",
            data.length(), table.c_str(), key.c_str(), cas, expiry);

  std::string fqkey = get_fq_key(table, key);

  // Check whether this request is too big.  Note that neither Rogers nor
  // memcached impose a limit on the maximum request length, but there is no
  // legitimate case for needing to store more than this maximum and permitting
  // it opens us up to DoS attacks where e.g. a subscriber firing in rapid
  // REGISTERs leads to us storing arbitrarily large values in memcached.
  uint32_t data_length = data.length();
  if (data_length > Store::MAX_DATA_LENGTH)
  {
    if (trail != 0)
    {
      SAS::Event err(trail, SASEvent::MEMCACHED_REQ_TOO_LARGE, 0);
      err.add_var_param(fqkey);
      err.add_static_param(data_length);
      SAS::report_event(err);
    }

    TRC_INFO("Attempting to write more than %lu bytes of data -- reject request",
             Store::MAX_DATA_LENGTH);
    return Store::Status::ERROR;
  }

  if (trail != 0)
  {
    int event;

    if (log_body)
    {
      event = SASEvent::MEMCACHED_SET_START;
    }
    else
    {
      event = SASEvent::MEMCACHED_SET_WITHOUT_DATA_START;
    }

    SAS::Event start(trail, event, 0);
    start.add_var_param(fqkey);
    start.add_static_param(cas);
    start.add_static_param(expiry);

    if (log_body)
    {
      // Note that we do this _after_ policing the maximum length which means
      // that data is less than the maximum 64k supported by SAS.
      start.add_var_param(data);
      start.add_static_param(data_format);
    }
  
    SAS::report_event(start);
  }

  memcached_store_func f =
    [&] (ConnectionHandle<memcached_st*>& conn_handle,
         time_t memcached_expiration) -> memcached_return_t
  {
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
      CW_IO_STARTS("Memcached CAS for " + fqkey)
      {
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
      CW_IO_COMPLETES()
    }
    return rc;
  };

  return set_data(fqkey,
                  data,
                  expiry,
                  trail,
                  f);
}

Store::Status TopologyNeutralMemcachedStore::set_data_without_cas(const std::string& table,
                                                                  const std::string& key,
                                                                  const std::string& data,
                                                                  int expiry,
                                                                  SAS::TrailId trail,
                                                                  bool log_body,
                                                                  Store::Format data_format)
{
  TRC_DEBUG("Writing %d bytes to table %s key %s, expiry = %d",
            data.length(), table.c_str(), key.c_str(), expiry);

  std::string fqkey = get_fq_key(table, key);

  if (trail != 0)
  {
    int event;

    if (log_body)
    {
      event = SASEvent::MEMCACHED_SET_WITHOUT_CAS_START;
    }
    else
    {
      event = SASEvent::MEMCACHED_SET_WITHOUT_DATA_OR_CAS_START;
    }

    SAS::Event start(trail, event, 0);
    start.add_var_param(fqkey);
    start.add_static_param(expiry);

    if (log_body)
    {
      start.add_var_param(data);
      start.add_static_param(data_format);
    }

    SAS::report_event(start);
  }

  memcached_store_func f =
    [&] (ConnectionHandle<memcached_st*>& conn_handle,
         time_t memcached_expiration) -> memcached_return_t
  {
    memcached_return_t rc;

    CW_IO_STARTS("Memcached SET for " + fqkey)
    {
      rc = memcached_set_vb(conn_handle.get_connection(),
                            fqkey.data(),
                            fqkey.length(),
                            0,
                            data.data(),
                            data.length(),
                            memcached_expiration,
                            0);
    }
    CW_IO_COMPLETES();

    return rc;
  };

  return set_data(fqkey,
                  data,
                  expiry,
                  trail,
                  f);
}

Store::Status TopologyNeutralMemcachedStore::set_data(const std::string& fqkey,
                                                      const std::string& data,
                                                      int expiry,
                                                      SAS::TrailId trail,
                                                      memcached_store_func f)
{
  Store::Status status = Store::Status::OK;
  std::vector<AddrInfo> targets;
  memcached_return_t rc;

  // Memcached uses a flexible mechanism for specifying expiration.
  // - 0 indicates never expire.
  // - <= MEMCACHED_EXPIRATION_MAXDELTA indicates a relative (delta) time.
  // - > MEMCACHED_EXPIRATION_MAXDELTA indicates an absolute time.
  // Absolute time is the only way to force immediate expiry.  Unfortunately,
  // it's not reliable (e.g. as a result of NTP changes). Instead, we use
  // relative time for future times (expiry > 0) and the earliest absolute
  // time for immediate expiry (expiry == 0).
  time_t memcached_expiration =
    (time_t)((expiry > 0) ? expiry : MEMCACHED_EXPIRATION_MAXDELTA + 1);

  if (!get_targets(targets, trail))
  {
    return ERROR;
  }

  // Set to each replica (mechansim determined by the update function), stopping if we
  // get a definitive success/failure response.
  //
  // The code that does the operation is passed as a lambda that captures all
  // necessary variables by reference.
  memcached_func f1 = std::bind(f,
                                std::placeholders::_1,
                                memcached_expiration);
  rc = iterate_through_targets(targets, trail, f1);

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
      CW_IO_STARTS("Memcached DELETE for " + fqkey)
      {
        rc = memcached_delete(conn_handle.get_connection(), fqkey.data(), fqkey.length(), 0);
      }
      CW_IO_COMPLETES()
    }
    else
    {
      CW_IO_STARTS("Memcached SET for " + fqkey)
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
      CW_IO_COMPLETES()
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
