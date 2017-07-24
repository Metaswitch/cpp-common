/**
 * @file snmp_time_and_string_based_event_table.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "snmp_internal/snmp_includes.h"
#include "snmp_internal/snmp_time_period_and_string_table.h"
#include "snmp_time_and_string_based_event_table.h"
#include "event_statistic_accumulator.h"
#include <vector>
#include "log.h"

namespace SNMP
{

// Time and String indexed Row that maps the data from EventStatisticAccumulator
// into the right columns.
class TimeAndStringBasedEventRow: public TimeAndStringBasedRow<EventStatisticAccumulator>
{
public:
  TimeAndStringBasedEventRow(int time_index, std::string str_index, View* view):
    TimeAndStringBasedRow<EventStatisticAccumulator>(time_index, str_index, view) {};

  ColumnData get_columns()
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    // Call into the underlying EventStatisticsAccumulator to get the current
    // statistics.
    EventStatistics statistics;
    EventStatisticAccumulator* accumulated = _view->get_data(now);
    accumulated->get_stats(statistics);

    // Construct and return a ColumnData with the appropriate values.
    ColumnData ret;
    ret[1] = Value::integer(this->_index);
    ret[2] = Value(ASN_OCTET_STR,
                   (unsigned char*)(this->_string_index.c_str()),
                   this->_string_index.size());
    ret[3] = Value::uint(statistics.mean);
    ret[4] = Value::uint(statistics.variance);
    ret[5] = Value::uint(statistics.hwm);
    ret[6] = Value::uint(statistics.lwm);
    ret[7] = Value::uint(statistics.count);
    return ret;
  }
};

class TimeAndStringBasedEventTableImpl: public ManagedTable<TimeAndStringBasedEventRow, int>, public TimeAndStringBasedEventTable
{
public:
  TimeAndStringBasedEventTableImpl(std::string name,
                                   std::string tbl_oid):
    ManagedTable<TimeAndStringBasedEventRow, int>(name,
                                                  tbl_oid,
                                                  3,
                                                  7,
                                                  { ASN_INTEGER , ASN_OCTET_STR })
  {
    _table_rows = 0;

    // Create a lock to protect the maps in this table.  Note that our policy
    // for stats is to minimize locking.  We are prepared to tolerate some
    // invalid statistics readings in order to avoid locking on the call path.
    // However, we don't want to risk conflicting mutates of the maps that
    // track all of the underlying data from completely breaking all stats.
    pthread_rwlock_init(&_table_lock, NULL);
  }

  /// Add rows to the table for a given string index (one row per time period
  /// that we are tracking.
  void create_and_add_rows(std::string string_index)
  {
    // This is going to mutate the maps so get the write lock first.
    pthread_rwlock_wrlock(&_table_lock);

    // Check that the rows don't already exist.  We only call this function if
    // the rows don't exist, but it's possible that they've been added
    // inbetween us checking that they don't exist and getting the write lock.
    if (_five_second.count(string_index) != 0)
    {
      // The rows already exist.  We don't release the RW lock until we've
      // completely finished adding all the rows so existence in the 5s map
      // means that all the rows are fully created.  Just return.
      pthread_rwlock_unlock(&_table_lock);
      return;
    }

    // Create CurrentAndPrevious views of EventStatisticAccumulator to
    // accumulate statistics for these rows at each of 5s and 5m scopes.
    _five_second[string_index] = new CurrentAndPrevious<EventStatisticAccumulator>(5000);
    _five_minute[string_index] = new CurrentAndPrevious<EventStatisticAccumulator>(300000);

    // Now add the actual rows to the table, referencing the
    // EventStatisticAccumulator views we've just created.  Note that we create
    // the rows with an abitrary internal key (using a _table_rows counter to
    // generate unique keys) as we don't need to be able to look them up again
    // in future.  We don't need this because we don't currently support
    // removing rows from the table.  If we need to add this in future then we
    // would need to start keying the rows off a tuple of string index and time
    // scope.
    this->add(_table_rows++, new TimeAndStringBasedEventRow(TimePeriodIndexes::scopePrevious5SecondPeriod,
                                    string_index,
                                    new TimeAndStringBasedEventRow::PreviousView((_five_second)[string_index])));
    this->add(_table_rows++, new TimeAndStringBasedEventRow(TimePeriodIndexes::scopeCurrent5MinutePeriod,
                                    string_index,
                                    new TimeAndStringBasedEventRow::CurrentView((_five_minute)[string_index])));
    this->add(_table_rows++, new TimeAndStringBasedEventRow(TimePeriodIndexes::scopePrevious5MinutePeriod,
                                    string_index,
                                    new TimeAndStringBasedEventRow::PreviousView((_five_minute)[string_index])));

    pthread_rwlock_unlock(&_table_lock);
  }

  void accumulate(std::string string_index, uint32_t sample)
  {
    // First of all check to see if we have already created rows for this
    // index.  If we have created rows then we will also have created entries
    // in our _five_second and _five_minute maps for storing the data.  Check
    // one of them.
    bool rows_exist = false;

    // We want to make sure that we get this right -- deciding that the rows
    // exist when actually they don't would be bad.  Get the read lock first.
    pthread_rwlock_rdlock(&_table_lock);
    // We've got the read lock so no one can have a write lock.  Existence
    // of the string_index in the 5s map therefore means we can safely assume
    // that all of the rows for this string_index exist.
    rows_exist = (_five_second.count(string_index) != 0);
    pthread_rwlock_unlock(&_table_lock);

    if (!rows_exist)
    {
      TRC_DEBUG("Create new rows for %s", string_index.c_str());
      create_and_add_rows(string_index);
    }

    // The rows definitely exist now, so go ahead and accumulate the counts.
    // We don't bother about locking the table here -- the worst that will
    // happen is that the stats might be slightly wrong.
    _five_second[string_index]->get_current()->accumulate(sample);
    _five_minute[string_index]->get_current()->accumulate(sample);
  }

  ~TimeAndStringBasedEventTableImpl()
  {
    pthread_rwlock_destroy(&_table_lock);

    //Spin through the maps of 5s and 5m views and delete them.
    for (auto& kv : _five_second) {  delete kv.second; }
    for (auto& kv : _five_minute) {  delete kv.second; }
  }
private:

  TimeAndStringBasedEventRow* new_row(int indexes) { return NULL;};

  // The number of rows in the table -- used to assign a unique (but arbitrary)
  // index to each row.
  int _table_rows;

  // Maps for tracking our 5s and 5m views of the data for each string index
  // used in the table, plus a lock to protect them.
  pthread_rwlock_t _table_lock;
  std::map<std::string, CurrentAndPrevious<EventStatisticAccumulator>*> _five_second;
  std::map<std::string, CurrentAndPrevious<EventStatisticAccumulator>*> _five_minute;
};

TimeAndStringBasedEventTable* TimeAndStringBasedEventTable::create(std::string name,
                                       std::string oid)
{
  return new TimeAndStringBasedEventTableImpl(name, oid);
}

}
