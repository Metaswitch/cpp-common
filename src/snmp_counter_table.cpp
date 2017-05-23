/**
 * @file snmp_counter_table.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "snmp_counter_table.h"
#include "snmp_statistics_structures.h"
#include "snmp_internal/snmp_includes.h"
#include "snmp_internal/snmp_time_period_table.h"
#include "logger.h"

namespace SNMP
{

// Just a TimeBasedRow that maps the data from SingleCount into the right column.
class CounterRow: public TimeBasedRow<SingleCount>
{
public:
  CounterRow(int index, View* view):
    TimeBasedRow<SingleCount>(index, view) {};
  ColumnData get_columns()
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    SingleCount accumulated = *(this->_view->get_data(now));

    // Construct and return a ColumnData with the appropriate values
    ColumnData ret;
    ret[1] = Value::integer(this->_index);
    ret[2] = Value::uint(accumulated.count);
    return ret;
  }
};

class CounterTableImpl: public ManagedTable<CounterRow, int>, public CounterTable
{
public:
  CounterTableImpl(std::string name,
                   std::string tbl_oid):
    ManagedTable<CounterRow, int>(name,
                                  tbl_oid,
                                  2,
                                  2, // Only column 2 should be visible
                                  { ASN_INTEGER }), // Type of the index column
    five_second(5000),
    five_minute(300000)
  {
    // We have a fixed number of rows, so create them in the constructor.
    add(TimePeriodIndexes::scopePrevious5SecondPeriod);
    add(TimePeriodIndexes::scopeCurrent5MinutePeriod);
    add(TimePeriodIndexes::scopePrevious5MinutePeriod);
  }

  void increment()
  {
    // Increment each underlying set of data.
    five_second.get_current()->count++;
    five_minute.get_current()->count++;
  }

private:
  // Map row indexes to the view of the underlying data they should expose
  CounterRow* new_row(int index)
  {
    CounterRow::View* view = NULL;
    switch (index)
    {
      case TimePeriodIndexes::scopePrevious5SecondPeriod:
        view = new CounterRow::PreviousView(&five_second);
        break;
      case TimePeriodIndexes::scopeCurrent5MinutePeriod:
        view = new CounterRow::CurrentView(&five_minute);
        break;
      case TimePeriodIndexes::scopePrevious5MinutePeriod:
        view = new CounterRow::PreviousView(&five_minute);
        break;
    }
    return new CounterRow(index, view);
  }

  CurrentAndPrevious<SingleCount> five_second;
  CurrentAndPrevious<SingleCount> five_minute;
};

CounterTable* CounterTable::create(std::string name, std::string oid)
{
  return new CounterTableImpl(name, oid);
}

}
