/**
 * @file cassandra_connection_pool.h  Declaration of derived class for Cassandra
 * connection pooling.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef CASSANDRA_CONNECTION_POOL_H__
#define CASSANDRA_CONNECTION_POOL_H__

#include "thrift/Thrift.h"
#include "thrift/transport/TSocket.h"
#include "thrift/transport/TTransport.h"
#include "thrift/transport/TBufferTransports.h"
#include "thrift/protocol/TProtocol.h"
#include "thrift/protocol/TBinaryProtocol.h"

#include "connection_pool.h"

using namespace apache::thrift;
using namespace apache::thrift::transport;
using namespace apache::thrift::protocol;

namespace CassandraStore
{
class Client;

class CassandraConnectionPool : public ConnectionPool<Client*>
{
public:
  CassandraConnectionPool();

  ~CassandraConnectionPool()
  {
    // This call is important to properly destroy the connection pool
    destroy_connection_pool();
  }

protected:
  Client* create_connection(AddrInfo target) override;

  void destroy_connection(AddrInfo target, Client* conn) override;

  long _timeout_ms;
};

} // namespace CassandraStore
#endif
