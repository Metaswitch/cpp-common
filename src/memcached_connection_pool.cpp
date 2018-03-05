/**
 * @file memcachedconnectionpool.cpp  Implementation of derived class for
 * memcached connection pooling
 *
 * Copyright (C) Metaswitch Networks 2016
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

  // Disable Nagle's algorithm
  // (https://en.wikipedia.org/wiki/Nagle%27s_algorithm). If we leave it on
  // there can be up to 500ms delay between this code sending an
  // asynchronous SET and it actually being sent on the wire, e.g.
  //
  // * Ask libmemcached to do async SET.
  // * Async SET sent on the wire.
  // * Ask libmemcached to do a 2nd async SET.
  // * Up to 500ms passes.
  // * TCP stack receives ACK to 1st SET (may be delayed because the server
  //   does not send a protocol level response to the async SET).
  // * 2nd async SET sent on the wire (up to 500ms late).
  //
  // This delay can open up window conditions in failure scenarios. In
  // addition there is not much point in using Nagle. libmemcached's buffers
  // are large enough that it will never send small message fragments, and we
  // very rarely pipeline requests.
  memcached_behavior_set(conn,
                         MEMCACHED_BEHAVIOR_TCP_NODELAY,
                         true);

  std::string address = target.address.to_string();

  CW_IO_STARTS("Memcached Server Add for " + address)
  {
    memcached_server_add(conn, address.c_str(), target.port);
  }
  CW_IO_COMPLETES()

  return conn;
}

void MemcachedConnectionPool::destroy_connection(AddrInfo target, memcached_st* conn)
{
  memcached_free(conn);
}
// LCOV_EXCL_STOP
