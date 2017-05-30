/**
 * @file mock_cassandra_store.h Mock for cassandra store objects.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
