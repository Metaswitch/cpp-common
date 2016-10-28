/**
 * @file mock_cassandra_store.h Mock for cassandra store objects.
 *
 * Project Clearwater - IMS in the cloud.
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

#ifndef MOCK_CASSANDRA_STORE_H__
#define MOCK_CASSANDRA_STORE_H__

#include "gmock/gmock.h"
#include "cassandra_store.h"

using ::testing::SetArgReferee;
using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArgs;


MATCHER_P(PointerRefTo, ptr, "")
{
  return (arg == ptr);
}


class MockOperationMixin
{
public:
  MockOperationMixin() : trx(NULL) {};
  virtual ~MockOperationMixin()
  {
    delete trx; trx = NULL;
  };

  void set_trx(CassandraStore::Transaction* trx_param) { trx = trx_param; }
  CassandraStore::Transaction* get_trx() { return trx; }

  CassandraStore::Transaction* trx;
};


#define EXPECT_DO_ASYNC(STORE, MOCK_OP)                                        \
  {                                                                            \
    CassandraStore::Operation* op_ptr = NULL;                                  \
    CassandraStore::Transaction* trx_ptr = NULL;                               \
                                                                               \
    EXPECT_CALL((STORE), do_async(PointerRefTo(dynamic_cast<CassandraStore::Operation*>(&(MOCK_OP))), _))\
      .WillOnce(DoAll(WithArgs<1>(Invoke(&(MOCK_OP), &MockOperationMixin::set_trx)), \
                      SetArgReferee<0>(op_ptr),                                \
                      SetArgReferee<1>(trx_ptr)));                             \
  }


// Mock cassandra client that emulates the interface tot he C++ thrift bindings.
class MockCassandraClient : public CassandraStore::Client
{
public:
  MOCK_METHOD1(set_keyspace, void(const std::string& keyspace));
  MOCK_METHOD2(batch_mutate, void(const std::map<std::string, std::map<std::string, std::vector<cass::Mutation> > > & mutation_map, const cass::ConsistencyLevel::type consistency_level));
  MOCK_METHOD5(get_slice, void(std::vector<cass::ColumnOrSuperColumn> & _return, const std::string& key, const cass::ColumnParent& column_parent, const cass::SlicePredicate& predicate, const cass::ConsistencyLevel::type consistency_level));
  MOCK_METHOD5(multiget_slice, void(std::map<std::string, std::vector<cass::ColumnOrSuperColumn> > & _return, const std::vector<std::string>& keys, const cass::ColumnParent& column_parent, const cass::SlicePredicate& predicate, const cass::ConsistencyLevel::type consistency_level));
  MOCK_METHOD4(remove, void(const std::string& key, const cass::ColumnPath& column_path, const int64_t timestamp, const cass::ConsistencyLevel::type consistency_level));
  MOCK_METHOD0(connect, void());
  MOCK_METHOD0(is_connected, bool());
  MOCK_METHOD5(get_range_slices, void(std::vector<cass::KeySlice> & _return,
                                      const cass::ColumnParent& column_parent,
                                      const cass::SlicePredicate& predicate,
                                      const cass::KeyRange& range,
                                      const cass::ConsistencyLevel::type consistency_level));
};


#endif
