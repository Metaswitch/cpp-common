/**
 * @file memcachedconnectionpool.cpp  Implementation of derived class for
 * memcached connection pooling
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "memcached_connection_pool.h"

// LCOV_EXCL_START
memcached_st* MemcachedConnectionPool::create_connection(AddrInfo target)
{
  // Create and set up a memcached connection
  memcached_st* conn = memcached(_options.c_str(), _options.length());
  memcached_behavior_set(conn,
                         MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT,
                         _max_connect_latency_ms);
  memcached_server_add(conn, target.address.to_string().c_str(), target.port);

  return conn;
}

void MemcachedConnectionPool::destroy_connection(AddrInfo target, memcached_st* conn)
{
  memcached_free(conn);
}
// LCOV_EXCL_STOP
