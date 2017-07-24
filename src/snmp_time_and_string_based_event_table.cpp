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


// Time and String Indexed Row that maps the data from EventStatisticAccumulator into the right columns.
class TimeAndStringBasedEventRow: public TimeAndStringBasedRow<EventStatisticAccumulator>
{
public:
  TimeAndStringBasedEventRow(int time_index, std::string str_index, View* view):
    TimeAndStringBasedRow<EventStatisticAccumulator>(time_index, str_index, view) {};

  ColumnData get_columns()
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    TRC_DEBUG("Get stats for string %s", _string_index.c_str());

    EventStatistics statistics;
    EventStatisticAccumulator* accumulated = _view->get_data(now);
    accumulated->get_stats(statistics);

    TRC_DEBUG("Count = %u, Mean = %u", statistics.count, statistics.mean);
    TRC_DEBUG("String index = %s", (unsigned char*)(this->_string_index.c_str()));
    TRC_DEBUG("String length = %u", this->_string_index.size());
    TRC_DEBUG("Time index = %d", this->_index);

    // Construct and return a ColumnData with the appropriate values
    ColumnData ret;
    ret[1] = Value::integer(this->_index);
    ret[2] = Value(ASN_OCTET_STR, (unsigned char*)(this->_string_index.c_str()), this->_string_index.size());
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
    pthread_rwlock_init(&_table_lock, NULL);
  }

  void create_and_add_rows(std::string string_index)
  {
    // Create CurrentAndPrevious views of EventStatisticAccumulator to
    // accumulate statistics for this row at each of 5s and 5m scopes.
    // This is going to mutate the maps so get the write lock first.
    pthread_rwlock_wrlock(&_table_lock);
    _five_second[string_index] = new CurrentAndPrevious<EventStatisticAccumulator>(5000);
    _five_minute[string_index] = new CurrentAndPrevious<EventStatisticAccumulator>(300000);
    pthread_rwlock_unlock(&_table_lock);

    // Now add the actual rows to the table, referencing the relevant views of
    // the data that we've just created.
    this->add(_table_rows++, new TimeAndStringBasedEventRow(TimePeriodIndexes::scopePrevious5SecondPeriod,
                                    string_index,
                                    new TimeAndStringBasedEventRow::PreviousView((_five_second)[string_index])));
    this->add(_table_rows++, new TimeAndStringBasedEventRow(TimePeriodIndexes::scopeCurrent5MinutePeriod,
                                    string_index,
                                    new TimeAndStringBasedEventRow::CurrentView((_five_minute)[string_index])));
    this->add(_table_rows++, new TimeAndStringBasedEventRow(TimePeriodIndexes::scopePrevious5MinutePeriod,
                                    string_index,
                                    new TimeAndStringBasedEventRow::PreviousView((_five_minute)[string_index])));
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
    rows_exist = (_five_second.count(string_index) != 0);
    pthread_rwlock_unlock(&_table_lock);

    if (!rows_exist)
    {
      TRC_DEBUG("Create new rows for %s", string_index.c_str());
      // We don't have rows for this index so create them now.
      create_and_add_rows(string_index);
    }

    TRC_DEBUG("Accumulate sample: %u", sample);
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
