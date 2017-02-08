/**
 * @file cassandra_connection_pool.cpp  Implementation of derived class for
 * Cassandra connection pooling.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2016 Metaswitch Networks Ltd
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

#include "cassandra_connection_pool.h"
#include "cassandra_store.h"

namespace CassandraStore
{

static const int TSOCKET_CONN_TIMEOUT_MS = 150;
static const int TSOCKET_RECV_TIMEOUT_MS = 150;
static const int TSOCKET_SEND_TIMEOUT_MS = 150;

// The length of time a connection can remain idle before it is removed from
// the pool
static const double MAX_IDLE_TIME_S = 60;

// LCOV_EXCL_START - UTs do not cover the creation/deletion on Clients
CassandraConnectionPool::CassandraConnectionPool() :
  ConnectionPool<Client*>(MAX_IDLE_TIME_S, true)
{
}

Client* CassandraConnectionPool::create_connection(AddrInfo target)
{
  // We require the address as a string
  char buf[100];
  const char *remote_ip = inet_ntop(target.address.af,
                                    &target.address.addr,
                                    buf,
                                    sizeof(buf));

  boost::shared_ptr<TSocket> socket =
    boost::shared_ptr<TSocket>(new TSocket(std::string(remote_ip), target.port));
  socket->setConnTimeout(TSOCKET_CONN_TIMEOUT_MS);
  socket->setRecvTimeout(TSOCKET_RECV_TIMEOUT_MS);
  socket->setSendTimeout(TSOCKET_SEND_TIMEOUT_MS);
  boost::shared_ptr<TFramedTransport> transport =
    boost::shared_ptr<TFramedTransport>(new TFramedTransport(socket));
  boost::shared_ptr<TProtocol> protocol =
     boost::shared_ptr<TBinaryProtocol>(new TBinaryProtocol(transport));

  return new RealThriftClient(protocol, transport);
}

void CassandraConnectionPool::destroy_connection(AddrInfo target, Client* conn)
{
  delete conn; conn = NULL;
}
// LCOV_EXCL_STOP

} // namespace CassandraStore
