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
#include "limits.h"

namespace SNMP
{

// Storage for the underlying data
struct EventStatistics
{
  std::atomic_uint_fast64_t count;
  std::atomic_uint_fast64_t sum;
  std::atomic_uint_fast64_t sqsum;
  std::atomic_uint_fast64_t hwm;
  std::atomic_uint_fast64_t lwm;

  void reset(uint64_t periodstart, EventStatistics* previous = NULL);
};

// Just a TimeBasedRow that maps the data from EventStatistics into the right five columns.
class EventAccumulatorRow: public TimeBasedRow<EventStatistics>
{
public:
  EventAccumulatorRow(int index, View* view): TimeBasedRow<EventStatistics>(index, view) {};
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

  void accumulate_internal(CurrentAndPrevious<EventStatistics>& data, uint32_t sample)
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    EventStatistics* current = data.get_current(now);

    current->count++;

    // Just keep a running total as we go along, so we can calculate the average and variance on
    // request
    current->sum += sample;
    current->sqsum += (sample * sample);

    // Update the low- and high-water marks.  In each case, we get the current
    // value, decide whether a change is required and then atomically swap it
    // if so, repeating if it was changed in the meantime.  Note that
    // compare_exchange_weak loads the current value into the expected value
    // parameter (lwm or hwm below) if the compare fails.
    uint_fast64_t lwm = current->lwm.load();
    while ((sample < lwm) &&
           (!current->lwm.compare_exchange_weak(lwm, sample)))
    {
      // Do nothing.
    }
    uint_fast64_t hwm = current->hwm.load();
    while ((sample > hwm) &&
           (!current->hwm.compare_exchange_weak(hwm, sample)))
    {
      // Do nothing.
    }
  };


  CurrentAndPrevious<EventStatistics> five_second;
  CurrentAndPrevious<EventStatistics> five_minute;
};


void EventStatistics::reset(uint64_t periodstart, EventStatistics* previous)
{
  count.store(0);
  sum.store(0);
  sqsum.store(0);
  lwm.store(ULONG_MAX);
  hwm.store(0);
}

ColumnData EventAccumulatorRow::get_columns()
{
  struct timespec now;
  clock_gettime(CLOCK_REALTIME_COARSE, &now);

  EventStatistics* accumulated = _view->get_data(now);
  uint_fast32_t count = accumulated->count.load();

  uint_fast32_t avg = 0;
  uint_fast32_t variance = 0;
  uint_fast32_t lwm = 0;
  uint_fast32_t hwm = 0;

  if (count > 0)
  {
    uint_fast64_t sum = accumulated->sum.load();
    uint_fast64_t sumsq = accumulated->sqsum.load();
    // Calculate the average and the variance from the stored sum and sum-of-squares.
    avg = sum/count;
    variance = ((sumsq * count) - (sum * sum)) / (count * count);
    hwm = accumulated->hwm.load();
    lwm = accumulated->lwm.load();
  }

  // Construct and return a ColumnData with the appropriate values
  ColumnData ret;
  ret[1] = Value::integer(_index);
  ret[2] = Value::uint(avg);
  ret[3] = Value::uint(variance);
  ret[4] = Value::uint(hwm);
  ret[5] = Value::uint(lwm);
  ret[6] = Value::uint(count);
  return ret;
}

EventAccumulatorTable* EventAccumulatorTable::create(std::string name, std::string oid)
{
  return new EventAccumulatorTableImpl(name, oid);

}
}
