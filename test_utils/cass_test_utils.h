/**
 * @file cass_test_utils.h Cassandra unit test utlities.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef CASS_TEST_UTILS_H_
#define CASS_TEST_UTILS_H_

#include <semaphore.h>
#include <time.h>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "test_interposer.hpp"
#include "fakelogger.h"

#include "cassandra_store.h"

using ::testing::PrintToString;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::Throw;
using ::testing::_;
using ::testing::Mock;
using ::testing::MakeMatcher;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;
using ::testing::Invoke;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Gt;
using ::testing::Lt;

namespace CassTestUtils
{

//
// TEST HARNESS CODE.
//

// Transaction object used by the testbed. This mocks the on_success and
// on_failure methods to allow testcases to control it's behaviour.
//
// The transaction is destroyed by the store on one of it's worker threads.
// When destroyed, this object posts to a semaphore which signals the main
// thread to continue executing the testcase.
class TestTransaction : public CassandraStore::Transaction
{
public:
  TestTransaction(sem_t* sem) :
    CassandraStore::Transaction(0), _sem(sem)
  {}

  virtual ~TestTransaction()
  {
    sem_post(_sem);
  }

  void check_latency(unsigned long expected_latency_us)
  {
    unsigned long actual_latency_us;
    bool rc;

    rc = get_duration(actual_latency_us);
    EXPECT_TRUE(rc);
    EXPECT_EQ(expected_latency_us, actual_latency_us);

    cwtest_advance_time_ms(1);

    rc = get_duration(actual_latency_us);
    EXPECT_TRUE(rc);
    EXPECT_EQ(expected_latency_us, actual_latency_us);
  }

  MOCK_METHOD1(on_success, void(CassandraStore::Operation*));
  MOCK_METHOD1(on_failure, void(CassandraStore::Operation*));

private:
  sem_t* _sem;
};


// A class (and interface) that records the result of a cassandra operation.
//
// In the template:
// -  O is the operation class.
// -  T is the type of data returned by get_result().
class ResultRecorderInterface
{
public:
  virtual void save(CassandraStore::Operation* op) = 0;
};

template<class O, class T>
class ResultRecorder : public ResultRecorderInterface
{
public:
  void save(CassandraStore::Operation* op)
  {
    dynamic_cast<O*>(op)->get_result(result);
  }

  T result;
};


// A specialized transaction that can be configured to record the result of a
// request on a recorder object.
class RecordingTransaction : public TestTransaction
{
public:
  RecordingTransaction(sem_t* sem,
                       ResultRecorderInterface* recorder) :
    TestTransaction(sem),
    _recorder(recorder)
  {}

  virtual ~RecordingTransaction() {}

  void record_result(CassandraStore::Operation* op)
  {
    _recorder->save(op);
  }

private:
  ResultRecorderInterface* _recorder;
};

//
// TYPE DEFINITIONS AND CONSTANTS
//

// A mutation map as used in batch_mutate(). This is of the form:
// { row: { table : [ Mutation ] } }.
typedef std::map<std::string, std::map<std::string, std::vector<cass::Mutation>>> mutmap_t;

// A mutation map in a more usable form. Structured as
//
// {
//   table: {
//     key: {
//       column_name: find_column_value
//     }
//   }
// }
typedef std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> nice_mutmap_t;

// A slice as returned by get_slice().
typedef std::vector<cass::ColumnOrSuperColumn> slice_t;

const slice_t empty_slice(0);

typedef std::map<std::string, std::vector<cass::ColumnOrSuperColumn>> multiget_slice_t;

const multiget_slice_t empty_slice_multiget;

// Utility functions to make a slice from a map of column names => values.
void make_slice(slice_t& slice,
                const std::map<std::string, std::string>& columns,
                int32_t ttl = 0)
{
  for(std::map<std::string, std::string>::const_iterator it = columns.begin();
      it != columns.end();
      ++it)
  {
    cass::Column c;
    c.__set_name(it->first);
    c.__set_value(it->second);
    if (ttl != 0)
    {
      c.__set_ttl(ttl);
    }

    cass::ColumnOrSuperColumn csc;
    csc.__set_column(c);

    slice.push_back(csc);
  }
}


//
// MATCHERS
//

// A class that matches against a supplied mutation map.
class MultipleCfMutationMapMatcher : public MatcherInterface<const mutmap_t&> {
public:
  MultipleCfMutationMapMatcher(const std::vector<CassandraStore::RowColumns>& expected):
    _expected(expected)
  {
  };

  virtual bool MatchAndExplain(const mutmap_t& mutmap,
                               MatchResultListener* listener) const
  {
    // First check we have the right number of rows.
    if (mutmap.size() != _expected.size())
    {
      *listener << "map has " << mutmap.size()
                << " rows, expected " << _expected.size();
      return false;
    }

    // Loop through the rows we expect and check that are all present in the
    // mutmap.
    for(std::vector<CassandraStore::RowColumns>::const_iterator expected = _expected.begin();
        expected != _expected.end();
        ++expected)
    {
      std::string row = expected->key;
      std::map<std::string, std::string> expected_columns = expected->columns;
      mutmap_t::const_iterator row_mut = mutmap.find(row);

      if (row_mut == mutmap.end())
      {
        *listener << row << " row expected but not present";
        return false;
      }

      if (row_mut->second.size() != 1)
      {
        *listener << "multiple tables specified for row " << row;
        return false;
      }

      // Get the table name being operated on (there can only be one as checked
      // above), and the mutations being applied to it for this row.
      const std::string& table = row_mut->second.begin()->first;
      const std::vector<cass::Mutation>& row_table_mut =
                                                row_mut->second.begin()->second;
      std::string row_table_name = row + ":" + table;

      // Check we're modifying the right table.
      if (table != expected->cf)
      {
        *listener << "wrong table for " << row
                  << "(expected " << expected->cf
                  << ", got " << table << ")";
        return false;
      }

      // Check we've modifying the right number of columns for this row/table.
      if (row_table_mut.size() != expected_columns.size())
      {
        *listener << "wrong number of columns for " << row_table_name
                  << "(expected " << expected_columns.size()
                  << ", got " << row_table_mut.size() << ")";
        return false;
      }

      for(std::vector<cass::Mutation>::const_iterator mutation = row_table_mut.begin();
          mutation != row_table_mut.end();
          ++mutation)
      {
        // We only allow mutations for a single column (not supercolumns,
        // counters, etc).
        if (!mutation->__isset.column_or_supercolumn ||
            mutation->__isset.deletion ||
            !mutation->column_or_supercolumn.__isset.column ||
            mutation->column_or_supercolumn.__isset.super_column ||
            mutation->column_or_supercolumn.__isset.counter_column ||
            mutation->column_or_supercolumn.__isset.counter_super_column)
        {
          *listener << row_table_name << " has a mutation that isn't a single column change";
          return false;
        }

        // By now we know we're dealing with a column mutation, so extract the
        // column itself and build a descriptive name.
        const cass::Column& column = mutation->column_or_supercolumn.column;
        const std::string row_table_column_name =
                                             row_table_name + ":" + column.name;

        // Check that we were expecting to receive this column and if we were,
        // extract the expected value.
        if (expected_columns.find(column.name) == expected_columns.end())
        {
          *listener << "unexpected mutation " << row_table_column_name;
          return false;
        }

        const std::string& expected_value = expected_columns.find(column.name)->second;

        // Check it specifies the correct value.
        if (!column.__isset.value)
        {
          *listener << row_table_column_name << " does not have a value";
          return false;
        }

        if (column.value != expected_value)
        {
          *listener << row_table_column_name
                    << " has wrong value (expected " << expected_value
                    << " , got " << column.value << ")";
          return false;
        }
      }
    }

    // Phew! All checks passed.
    return true;
  }

  // User friendly description of what we expect the mutmap to do.
  virtual void DescribeTo(::std::ostream* os) const
  {
  }

private:
  std::vector<CassandraStore::RowColumns> _expected;
};

// A class that matches against a supplied mutation map.
class BatchDeletionMatcher : public MatcherInterface<const mutmap_t&> {
public:
  BatchDeletionMatcher(const std::vector<CassandraStore::RowColumns>& expected):
    _expected(expected)
  {
  };

  virtual bool MatchAndExplain(const mutmap_t& mutmap,
                               MatchResultListener* listener) const
  {
    // First check we have the right number of rows.
    if (mutmap.size() != _expected.size())
    {
      *listener << "map has " << mutmap.size()
                << " rows, expected " << _expected.size();
      return false;
    }

    // Loop through the rows we expect and check that are all present in the
    // mutmap.
    for(std::vector<CassandraStore::RowColumns>::const_iterator expected = _expected.begin();
        expected != _expected.end();
        ++expected)
    {
      std::string row = expected->key;
      std::map<std::string, std::string> expected_columns = expected->columns;
      mutmap_t::const_iterator row_mut = mutmap.find(row);

      if (row_mut == mutmap.end())
      {
        *listener << row << " row expected but not present";
        return false;
      }

      if (row_mut->second.size() != 1)
      {
        *listener << "multiple tables specified for row " << row;
        return false;
      }

      // Get the table name being operated on (there can only be one as checked
      // above), and the mutations being applied to it for this row.
      const std::string& table = row_mut->second.begin()->first;
      const std::vector<cass::Mutation>& row_table_mut =
                                                row_mut->second.begin()->second;
      std::string row_table_name = row + ":" + table;

      // Check we're modifying the right table.
      if (table != expected->cf)
      {
        *listener << "wrong table for " << row
                  << "(expected " << expected->cf
                  << ", got " << table << ")";
        return false;
      }

      // Deletions should only consist of one mutation per row.
      if (row_table_mut.size() != 1)
      {
        *listener << "wrong number of columns for " << row_table_name
                  << "(expected 1"
                  << ", got " << row_table_mut.size() << ")";
        return false;
      }

      const cass::Mutation* mutation = &row_table_mut.front();
      // We only allow mutations for a single column (not supercolumns,
      // counters, etc).
      if (!mutation->__isset.deletion)
      {
        *listener << row_table_name << " has a mutation that isn't a deletion";
        return false;
      }

      const cass::SlicePredicate* deletion = &mutation->deletion.predicate;

      // Check that the number of columns to be deleted is right
      if (deletion->column_names.size() != expected_columns.size())
      {
        *listener << deletion->column_names.size() << " columns deleted, expected " << expected_columns.size();
        return false;
      }

      // Loop over the columns and check that each of them
      // is expected
      for(std::vector<std::string>::const_iterator col = deletion->column_names.begin();
          col != deletion->column_names.end();
          ++col)
      {
        // Check that we were expecting to receive this column and if we were,
        // extract the expected value.
        if (expected_columns.find(*col) == expected_columns.end())
        {
          *listener << "unexpected mutation " << *col;
          return false;
        }
      }

    }

    // Phew! All checks passed.
    return true;
  }

  // User friendly description of what we expect the mutmap to do.
  virtual void DescribeTo(::std::ostream* os) const
  {
  }

private:
  std::vector<CassandraStore::RowColumns> _expected;
};

class SliceDeletionMatcher : public MatcherInterface<const mutmap_t&> {
public:
  SliceDeletionMatcher(const std::string& key,
                       const std::string& table,
                       const std::string& start,
                       const std::string& finish) :
    _expected_key(key),
    _expected_table(table),
    _expected_start(start),
    _expected_finish(finish)
  {
  };

  virtual bool MatchAndExplain(const mutmap_t& mutmap,
                               MatchResultListener* listener) const
  {
    // First check we have the right number of rows. We currently only support
    // 1.
    const unsigned int expected_num_rows = 1;
    if (mutmap.size() != expected_num_rows)
    {
      *listener << "map has " << mutmap.size()
                << " rows, expected " << expected_num_rows;
      return false;
    }

    mutmap_t::const_iterator row_mut = mutmap.find(_expected_key);

    if (row_mut == mutmap.end())
    {
      *listener << _expected_key << " row expected but not present";
      return false;
    }
    const std::string row = row_mut->first;

    if (row_mut->second.size() != 1)
    {
      *listener << "multiple tables specified for row " << row;
      return false;
    }

    // Get the table name being operated on (there can only be one as checked
    // above), and the mutations being applied to it for this row.
    const std::string& table = row_mut->second.begin()->first;
    const std::vector<cass::Mutation>& row_table_mut =
                                                row_mut->second.begin()->second;
    std::string row_table_name = row + ":" + table;

    // Check we're modifying the right table.
    if (table != _expected_table)
    {
      *listener << "wrong table for " << _expected_key
                << "(expected " << _expected_table
                << ", got " << table << ")";
      return false;
    }

    // Deletions should only consist of one mutation per row.
    if (row_table_mut.size() != 1)
    {
      *listener << "wrong number of columns for " << row_table_name
                << "(expected 1"
                << ", got " << row_table_mut.size() << ")";
      return false;
    }

    const cass::Mutation* mutation = &row_table_mut.front();

    // We only allow mutations for a single column (not supercolumns,
    // counters, etc).
    if (!mutation->__isset.deletion)
    {
      *listener << row_table_name << " has a mutation that isn't a deletion";
      return false;
    }

    const cass::Deletion* deletion = &mutation->deletion;

    if (!deletion->__isset.timestamp)
    {
      *listener << "Deletion timestamp is not set";
      return false;
    }

    const cass::SlicePredicate* predicate = &deletion->predicate;

    if (!predicate->__isset.slice_range ||
        predicate->__isset.column_names)
    {
      *listener << "mutation deletes named columns, when a slice was expected";
      return false;
    }

    const cass::SliceRange* range = &predicate->slice_range;

    if (range->start != _expected_start)
    {
      *listener << "wrong range start (expected " << _expected_start
                << ", got " << range->start << ")";
      return false;
    }

    if (range->finish != _expected_finish)
    {
      *listener << "wrong range finish (expected " << _expected_finish
                << ", got " << range->finish << ")";
      return false;
    }

    if (range->reversed)
    {
      *listener << "Rows were requested in reversed order";
      return true;
    }

    // Phew! All checks passed.
    return true;
  }

  // User friendly description of what we expect the mutmap to do.
  virtual void DescribeTo(::std::ostream* os) const
  {
  }

private:
  const std::string _expected_key;
  const std::string _expected_table;
  const std::string _expected_start;
  const std::string _expected_finish;
};


// A class that matches against a supplied mutation map.
class MutationMapMatcher : public MatcherInterface<const mutmap_t&> {
public:
  MutationMapMatcher(const std::string& table,
                     const std::vector<std::string>& rows,
                     const std::map<std::string, std::pair<std::string, int32_t> >& columns,
                     int64_t timestamp,
                     int32_t ttl = 0) :
    _table(table),
    _rows(rows),
    _columns(columns),
    _timestamp(timestamp)
  {};

  MutationMapMatcher(const std::string& table,
                     const std::vector<std::string>& rows,
                     const std::map<std::string, std::string>& columns,
                     int64_t timestamp,
                     int32_t ttl = 0) :
    _table(table),
    _rows(rows),
    _timestamp(timestamp)
  {
    for(std::map<std::string, std::string>::const_iterator column = columns.begin();
        column != columns.end();
        ++column)
    {
      _columns[column->first].first = column->second;
      _columns[column->first].second = ttl;
    }
  };

  virtual bool MatchAndExplain(const mutmap_t& mutmap,
                               MatchResultListener* listener) const
  {
    // First check we have the right number of rows.
    if (mutmap.size() != _rows.size())
    {
      *listener << "map has " << mutmap.size()
                << " rows, expected " << _rows.size();
      return false;
    }

    // Loop through the rows we expect and check that are all present in the
    // mutmap.
    for(std::vector<std::string>::const_iterator row = _rows.begin();
        row != _rows.end();
        ++row)
    {
      mutmap_t::const_iterator row_mut = mutmap.find(*row);

      if (row_mut == mutmap.end())
      {
        *listener << *row << " row expected but not present";
        return false;
      }

      if (row_mut->second.size() != 1)
      {
        *listener << "multiple tables specified for row " << *row;
        return false;
      }

      // Get the table name being operated on (there can only be one as checked
      // above), and the mutations being applied to it for this row.
      const std::string& table = row_mut->second.begin()->first;
      const std::vector<cass::Mutation>& row_table_mut =
                                                row_mut->second.begin()->second;
      std::string row_table_name = *row + ":" + table;

      // Check we're modifying the right table.
      if (table != _table)
      {
        *listener << "wrong table for " << *row
                  << "(expected " << _table
                  << ", got " << table << ")";
        return false;
      }

      // Check we've modifying the right number of columns for this row/table.
      if (row_table_mut.size() != _columns.size())
      {
        *listener << "wrong number of columns for " << row_table_name
                  << "(expected " << _columns.size()
                  << ", got " << row_table_mut.size() << ")";
        return false;
      }

      for(std::vector<cass::Mutation>::const_iterator mutation = row_table_mut.begin();
          mutation != row_table_mut.end();
          ++mutation)
      {
        // We only allow mutations for a single column (not supercolumns,
        // counters, etc).
        if (!mutation->__isset.column_or_supercolumn ||
            mutation->__isset.deletion ||
            !mutation->column_or_supercolumn.__isset.column ||
            mutation->column_or_supercolumn.__isset.super_column ||
            mutation->column_or_supercolumn.__isset.counter_column ||
            mutation->column_or_supercolumn.__isset.counter_super_column)
        {
          *listener << row_table_name << " has a mutation that isn't a single column change";
          return false;
        }

        // By now we know we're dealing with a column mutation, so extract the
        // column itself and build a descriptive name.
        const cass::Column& column = mutation->column_or_supercolumn.column;
        const std::string row_table_column_name =
                                             row_table_name + ":" + column.name;

        // Check that we were expecting to receive this column and if we were,
        // extract the expected value.
        if (_columns.find(column.name) == _columns.end())
        {
          *listener << "unexpected mutation " << row_table_column_name;
          return false;
        }

        const std::string& expected_value = _columns.find(column.name)->second.first;
        const int32_t& expected_ttl = _columns.find(column.name)->second.second;

        // Check it specifies the correct value.
        if (!column.__isset.value)
        {
          *listener << row_table_column_name << " does not have a value";
          return false;
        }

        if (column.value != expected_value)
        {
          *listener << row_table_column_name
                    << " has wrong value (expected " << expected_value
                    << " , got " << column.value << ")";
          return false;
        }

        // The timestamp must be set and correct.
        if (!column.__isset.timestamp)
        {
          *listener << row_table_column_name << " timestamp is not set";
          return false;
        }

        if (column.timestamp != _timestamp)
        {
          *listener << row_table_column_name
                    << " has wrong timestamp (expected " << _timestamp
                    << ", got " << column.timestamp << ")";
          return false;
        }

        if (expected_ttl != 0)
        {
          // A TTL is expected. Check the field is present and correct.
          if (!column.__isset.ttl)
          {
            *listener << row_table_column_name << " ttl is not set";
            return false;
          }

          if (column.ttl != expected_ttl)
          {
            *listener << row_table_column_name
                      << " has wrong ttl (expected " << expected_ttl <<
                      ", got " << column.ttl << ")";
            return false;
          }
        }
        else
        {
          // A TTL is not expected, so check the field is not set.
          if (column.__isset.ttl)
          {
            *listener << row_table_column_name
                      << " ttl is incorrectly set (value is " << column.ttl << ")";
            return false;
          }
        }
      }
    }

    // Phew! All checks passed.
    return true;
  }

  // User friendly description of what we expect the mutmap to do.
  virtual void DescribeTo(::std::ostream* os) const
  {
    *os << "to write columns " << PrintToString(_columns) <<
           " to rows " << PrintToString(_rows) <<
           " in table " << _table;
  }

private:
  std::string _table;
  std::vector<std::string> _rows;
  std::map<std::string, std::pair<std::string, int32_t> > _columns;
  int64_t _timestamp;
};


// Utility functions for creating MutationMapMatcher objects.
inline Matcher<const mutmap_t&>
MutationMap(const std::string& table,
            const std::string& row,
            const std::map<std::string, std::string>& columns,
            int64_t timestamp,
            int32_t ttl = 0)
{
  std::vector<std::string> rows(1, row);
  return MakeMatcher(new MutationMapMatcher(table, rows, columns, timestamp, ttl));
}

inline Matcher<const mutmap_t&>
MutationMap(const std::string& table,
            const std::vector<std::string>& rows,
            const std::map<std::string, std::string>& columns,
            int64_t timestamp,
            int32_t ttl = 0)
{
  return MakeMatcher(new MutationMapMatcher(table, rows, columns, timestamp, ttl));
}

inline Matcher<const mutmap_t&>
MutationMap(const std::string& table,
            const std::string& row,
            const std::map<std::string, std::pair<std::string, int32_t> >& columns,
            int64_t timestamp)
{
  std::vector<std::string> rows(1, row);
  return MakeMatcher(new MutationMapMatcher(table, rows, columns, timestamp));
}

inline Matcher<const mutmap_t&>
MutationMap(const std::vector<CassandraStore::RowColumns>& expected)
{
  return MakeMatcher(new MultipleCfMutationMapMatcher(expected));
}

inline Matcher<const mutmap_t&>
DeletionMap(const std::vector<CassandraStore::RowColumns>& expected)
{
  return MakeMatcher(new BatchDeletionMatcher(expected));
}

inline Matcher<const mutmap_t&>
DeletionRange(const std::string& key,
              const std::string& table,
              const std::string& start,
              const std::string& finish)
{
  return MakeMatcher(new SliceDeletionMatcher(key, table, start, finish));
}

// Matcher that check whether the argument is a ColumnPath that refers to a
// single table.
MATCHER_P(ColumnPathForTable, table, std::string("refers to table ")+table)
{
  *result_listener << "refers to table " << arg.column_family;
  return (arg.column_family == table);
}

// Matcher that check whether the argument is a ColumnPath that refers to a
// single table.
MATCHER_P2(ColumnPath, table, column, std::string("refers to table ")+table)
{
  *result_listener << "refers to table " << arg.column_family;
  *result_listener << "refers to column " << arg.column;
  return ((arg.column_family == table) && (arg.column == column));
}


// Matcher that checks whether a SlicePredicate specifies a sequence of specific
// columns.
MATCHER_P(SpecificColumns,
          columns,
          std::string("specifies columns ")+PrintToString(columns))
{
  if (!arg.__isset.column_names || arg.__isset.slice_range)
  {
    *result_listener << "does not specify individual columns";
    return false;
  }

  // Compare the expected and received columns (sorting them before the
  // comparison to ensure a consistent order).
  std::vector<std::string> expected_columns = columns;
  std::vector<std::string> actual_columns = arg.column_names;

  std::sort(expected_columns.begin(), expected_columns.end());
  std::sort(actual_columns.begin(), actual_columns.end());

  if (expected_columns != actual_columns)
  {
    *result_listener << "specifies columns " << PrintToString(actual_columns);
    return false;
  }

  return true;
}

// Matcher that checks whether a SlicePredicate specifies all columns
MATCHER(AllColumns,
          std::string("requests all columns: "))
{
  if (arg.__isset.column_names || !arg.__isset.slice_range)
  {
    *result_listener << "does not request a slice range"; return false;
  }

  if (arg.slice_range.start != "")
  {
    *result_listener << "has incorrect start (" << arg.slice_range.start << ")";
    return false;
  }

  if (arg.slice_range.finish != "")
  {
    *result_listener << "has incorrect finish (" << arg.slice_range.finish << ")";
    return false;
  }

  return true;
}

// Matcher that checks whether a SlicePredicate specifies all columns with a
// particular prefix.
MATCHER_P(ColumnsWithPrefix,
          prefix,
          std::string("requests columns with prefix: ")+prefix)
{
  if (arg.__isset.column_names || !arg.__isset.slice_range)
  {
    *result_listener << "does not request a slice range"; return false;
  }

  if (arg.slice_range.start != prefix)
  {
    *result_listener << "has incorrect start (" << arg.slice_range.start << ")";
    return false;
  }

  // Calculate what the end of the range should be (the last byte should be
  // one more than the start - we don't handle wrapping since all current users
  // of cassandra deal with ASCII column names.
  std::string end_str = prefix;
  char last_char = *end_str.rbegin();
  last_char++;
  end_str = end_str.substr(0, end_str.length()-1) + std::string(1, last_char);

  if (arg.slice_range.finish != end_str)
  {
    *result_listener << "has incorrect finish (" << arg.slice_range.finish << ")";
    return false;
  }

  return true;
}

// Matcher that checks that a key range has the right start and end keys
// specified.
MATCHER_P2(KeysInRange,
           start_key,
           end_key,
           std::string("requests keys between ") + start_key + " and " + end_key)
{
  if (!arg.__isset.start_key || !arg.__isset.end_key)
  {
    *result_listener << "does not request a range of keys"; return false;
  }
  if (arg.__isset.start_token || arg.__isset.end_token)
  {
    *result_listener << "also specifies a token range"; return false;
  }

  if (start_key != arg.start_key)
  {
    *result_listener << "has incorrect start key (" << arg.start_key << ")"; return false;
  }
  if (start_key != arg.end_key)
  {
    *result_listener << "has incorrect end key (" << arg.end_key << ")"; return false;
  }

  return true;
}

// Matcher that checks that a key range specifies a particular maximum count.
MATCHER_P(KeyRangeWithCount, count, std::string("requests ") + std::to_string(count) + " keys")
{
  if (count != arg.count)
  {
    *result_listener << "has incorrect count (expected " << count << " got " << arg.count;
    return false;
  }

  return true;
}

ACTION_P(SaveMutmap, ptr)
{
  *ptr = arg0;
}

ACTION_P(SaveMutmapAsMap, ptr)
{
  ptr->clear();
  const mutmap_t& mutations = arg0;

  for (mutmap_t::const_iterator row_it = mutations.begin();
       row_it != mutations.end();
       ++row_it)
  {
    for (std::map<std::string, std::vector<cass::Mutation>>::const_iterator cf_it = row_it->second.begin();
         cf_it != row_it->second.end();
         ++cf_it)
    {
      for (std::vector<cass::Mutation>::const_iterator mut_it = cf_it->second.begin();
           mut_it != cf_it->second.end();
           ++mut_it)
      {
        ASSERT_TRUE(mut_it->__isset.column_or_supercolumn);
        ASSERT_TRUE(mut_it->column_or_supercolumn.__isset.column);
        const cass::Column& col = mut_it->column_or_supercolumn.column;
        (*ptr)[cf_it->first][row_it->first][col.name] = col.value;
      }
    }
  }
}

} // namespace CassTestUtils


#endif

