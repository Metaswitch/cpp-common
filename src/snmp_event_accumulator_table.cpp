/**
 * @file snmp_event_accumulator_table.cpp
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "snmp_internal/snmp_time_period_table.h"
#include "snmp_event_accumulator_table.h"
#include "event_statistic_accumulator.h"
#include "limits.h"

namespace SNMP
{

// Just a TimeBasedRow that maps the data from EventStatisticAccumulator into the right five columns.
class EventAccumulatorRow: public TimeBasedRow<EventStatisticAccumulator>
{
public:
  EventAccumulatorRow(int index, View* view): TimeBasedRow<EventStatisticAccumulator>(index, view) {};
  ColumnData get_columns();
};

class EventAccumulatorTableImpl: public ManagedTable<EventAccumulatorRow, int>, public EventAccumulatorTable
{
public:
  EventAccumulatorTableImpl(std::string name,
                       std::string tbl_oid):
    ManagedTable<EventAccumulatorRow, int>(name,
                                      tbl_oid,
                                      2,
                                      6, // Columns 2-6 should be visible
                                      { ASN_INTEGER }), // Type of the index column
    five_second(5000),
    five_minute(300000)
  {
    // We have a fixed number of rows, so create them in the constructor.
    add(TimePeriodIndexes::scopePrevious5SecondPeriod);
    add(TimePeriodIndexes::scopeCurrent5MinutePeriod);
    add(TimePeriodIndexes::scopePrevious5MinutePeriod);
  }

  // Accumulate a sample into the underlying statistics.
  void accumulate(uint32_t sample)
  {
    // Pass samples through to the underlying data structures
    accumulate_internal(five_second, sample);
    accumulate_internal(five_minute, sample);
  }

private:
  // Map row indexes to the view of the underlying data they should expose
  EventAccumulatorRow* new_row(int index)
  {
    EventAccumulatorRow::View* view = NULL;
    switch (index)
    {
      case TimePeriodIndexes::scopePrevious5SecondPeriod:
        view = new EventAccumulatorRow::PreviousView(&five_second);
        break;
      case TimePeriodIndexes::scopeCurrent5MinutePeriod:
        view = new EventAccumulatorRow::CurrentView(&five_minute);
        break;
      case TimePeriodIndexes::scopePrevious5MinutePeriod:
        view = new EventAccumulatorRow::PreviousView(&five_minute);
        break;
    }
    return new EventAccumulatorRow(index, view);
  }

  void accumulate_internal(CurrentAndPrevious<EventStatisticAccumulator>& data, uint32_t sample)
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    EventStatisticAccumulator* current = data.get_current(now);
    current->accumulate(sample);
  };


  CurrentAndPrevious<EventStatisticAccumulator> five_second;
  CurrentAndPrevious<EventStatisticAccumulator> five_minute;
};


ColumnData EventAccumulatorRow::get_columns()
{
  struct timespec now;
  clock_gettime(CLOCK_REALTIME_COARSE, &now);
  EventStatistics statistics;

  EventStatisticAccumulator* accumulated = _view->get_data(now);
  accumulated->get_stats(statistics);

  // Construct and return a ColumnData with the appropriate values
  ColumnData ret;
  ret[1] = Value::integer(_index);
  ret[2] = Value::uint(statistics.mean);
  ret[3] = Value::uint(statistics.variance);
  ret[4] = Value::uint(statistics.hwm);
  ret[5] = Value::uint(statistics.lwm);
  ret[6] = Value::uint(statistics.count);
  return ret;
}

EventAccumulatorTable* EventAccumulatorTable::create(std::string name, std::string oid)
{
  return new EventAccumulatorTableImpl(name, oid);

}
}
