/**
 * @file snmp_event_accumulator_by_scope_table.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "snmp_internal/snmp_time_period_table.h"
#include "snmp_internal/snmp_time_period_and_scope_table.h"
#include "snmp_event_accumulator_table.h"
#include "snmp_event_accumulator_by_scope_table.h"
#include "event_statistic_accumulator.h"
#include "limits.h"

namespace SNMP
{

// A TimeAndScopeBasedRow that maps the data from EventStatisticAccumulator into the right five columns.
class EventAccumulatorByScopeRow: public TimeAndScopeBasedRow<EventStatisticAccumulator>
{
public:
  EventAccumulatorByScopeRow(int time_index, std::string scope_index, View* view): TimeAndScopeBasedRow<EventStatisticAccumulator>(time_index, scope_index, view) {};
  ColumnData get_columns();
};

class EventAccumulatorByScopeTableImpl: public ManagedTable<EventAccumulatorByScopeRow, int>, public EventAccumulatorByScopeTable
{
public:
  EventAccumulatorByScopeTableImpl(std::string name,
                                   std::string tbl_oid):
    ManagedTable<EventAccumulatorByScopeRow, int>(name,
                                                  tbl_oid,
                                                  3,
                                                  7, // Columns 3-7 should be visible
                                                  { ASN_INTEGER , ASN_OCTET_STR }), // Type of the index column
    five_second(5000),
    five_minute(300000)
  {
    // We have a fixed number of rows, so create them in the constructor.
    n = 0;

    this->add(n++, new_row(scopePrevious5SecondPeriod));
    this->add(n++, new_row(scopeCurrent5MinutePeriod));
    this->add(n++, new_row(scopePrevious5MinutePeriod));
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
  EventAccumulatorByScopeRow* new_row(int index)
  {
    EventAccumulatorByScopeRow::View* view = NULL;
    switch (index)
    {
      case TimePeriodIndexes::scopePrevious5SecondPeriod:
        view = new EventAccumulatorByScopeRow::PreviousView(&five_second);
        break;
      case TimePeriodIndexes::scopeCurrent5MinutePeriod:
        view = new EventAccumulatorByScopeRow::CurrentView(&five_minute);
        break;
      case TimePeriodIndexes::scopePrevious5MinutePeriod:
        view = new EventAccumulatorByScopeRow::PreviousView(&five_minute);
        break;
    }

    return new EventAccumulatorByScopeRow(index, "node", view);
  }

  void accumulate_internal(CurrentAndPrevious<EventStatisticAccumulator>& data, uint32_t sample)
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    EventStatisticAccumulator* current = data.get_current(now);
    current->accumulate(sample);
  };

  int n;

  CurrentAndPrevious<EventStatisticAccumulator> five_second;
  CurrentAndPrevious<EventStatisticAccumulator> five_minute;
};

ColumnData EventAccumulatorByScopeRow::get_columns()
{
  struct timespec now;
  clock_gettime(CLOCK_REALTIME_COARSE, &now);
  EventStatistics statistics;

  EventStatisticAccumulator* accumulated = _view->get_data(now);
  accumulated->get_stats(statistics);

  // Construct and return a ColumnData with the appropriate values
  ColumnData ret;
  ret[3] = Value::uint(statistics.mean);
  ret[4] = Value::uint(statistics.variance);
  ret[5] = Value::uint(statistics.hwm);
  ret[6] = Value::uint(statistics.lwm);
  ret[7] = Value::uint(statistics.count);

  return ret;
}

EventAccumulatorByScopeTable* EventAccumulatorByScopeTable::create(std::string name, std::string oid)
{
  return new EventAccumulatorByScopeTableImpl(name, oid);
}

}
