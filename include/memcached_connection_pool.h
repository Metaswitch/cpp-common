/**
 * @file memcachedconnectionpool.h  Declaration of derived class for memcached
 * connection pooling
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MEMCACHED_CONNECTION_POOL_H__
#define MEMCACHED_CONNECTION_POOL_H__

// Compilation fails when surrounding these two includes with extern "C", due to
// what seems to be conflicting definitions of a C function. It is unknown why
// this fails here but works in memcachedstore.h, but it appears to work without
// extern "C". Beware that omitting this may cause problems in future.
#include <libmemcached/memcached.h>
#include <libmemcached/util.h>

#include "connection_pool.h"

/// The length of time to allow for a memcached connection before
/// timing it out. This needs to be larger for remote sites.
///
/// Note that libmemcached can block for a relatively long time when trying to
/// read / write to an instance of memcached that is unavailable.  The worst
/// case scenario is if there is not an existing connection - in this case it
/// will block for three times the connect latency (which is one of the following
/// two values): once when trying to create the connection, and then twice
/// trying to use it (because libmemcached doesn't pass back the error).
static int LOCAL_MEMCACHED_CONNECTION_LATENCY_MS = 25;
static int REMOTE_MEMCACHED_CONNECTION_LATENCY_MS = 250;

class MemcachedConnectionPool : public ConnectionPool<memcached_st*>
{
public:
  MemcachedConnectionPool(time_t max_idle_time_s,
                          std::string options,
                          bool remote_store) :
    ConnectionPool<memcached_st*>(max_idle_time_s),
    _options(options),
    _max_connect_latency_ms(remote_store ? REMOTE_MEMCACHED_CONNECTION_LATENCY_MS :
                                           LOCAL_MEMCACHED_CONNECTION_LATENCY_MS)
  {
  }

  ~MemcachedConnectionPool()
  {
    // This call is important to properly destroy the connection pool
    destroy_connection_pool();
  }

protected:
  memcached_st* create_connection(AddrInfo target);
  void destroy_connection(AddrInfo target, memcached_st* conn);

  std::string _options;

  // The time to wait before timing out a connection to memcached.
  // (This is only used during normal running - at start-of-day we use
  // a fixed 10ms time, to start up as quickly as possible).
  unsigned int _max_connect_latency_ms;
};

#endif
