/**
 * @file mock_cassandra_connection_pool.h Mock CassandraConnectionPool for
 * testing.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "cassandra_connection_pool.h"

class MockCassandraConnectionPool : public CassandraStore::CassandraConnectionPool
{
public:
  MockCassandraConnectionPool(){}
  ~MockCassandraConnectionPool()
  {
  }

  MOCK_METHOD0(get_client, CassandraStore::Client*());

protected:
  void release_connection(ConnectionInfo<CassandraStore::Client*>* info, bool return_to_pool)
  {
    delete info; info = NULL;
  }

  ConnectionHandle<CassandraStore::Client*> get_connection(AddrInfo target)
  {
    CassandraStore::Client* client = get_client();
    ConnectionInfo<CassandraStore::Client*>* info =
      new ConnectionInfo<CassandraStore::Client*>(client, target);
    return ConnectionHandle<CassandraStore::Client*>(info, this);
  }
};
