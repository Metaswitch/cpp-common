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

class MemcachedConnectionPool : public ConnectionPool<memcached_st*>
{
public:
  MemcachedConnectionPool(time_t max_idle_time_s, std::string options) :
    ConnectionPool<memcached_st*>(max_idle_time_s),
    _options(options),
    _max_connect_latency_ms(50)
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
  unsigned int _max_connect_latency_ms;
};

#endif
