/**
 * @file snmp_event_accumulator_by_scope_table.cpp
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

#include "snmp_internal/snmp_time_period_table.h"
#include "snmp_internal/snmp_time_period_and_scope_table.h"
#include "snmp_event_accumulator_table.h"
#include "snmp_event_accumulator_by_scope_table.h"
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

// A TimeAndScopeBasedRow that maps the data from EventStatistics into the right five columns.
class EventAccumulatorByScopeRow: public TimeAndScopeBasedRow<EventStatistics>
{
public:
  EventAccumulatorByScopeRow(int time_index, std::string scope_index, View* view): TimeAndScopeBasedRow<EventStatistics>(time_index, scope_index, view) {};
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

  int n;

  CurrentAndPrevious<EventStatistics> five_second;
  CurrentAndPrevious<EventStatistics> five_minute;
};

ColumnData EventAccumulatorByScopeRow::get_columns()
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
  ret[3] = Value::uint(avg);
  ret[4] = Value::uint(variance);
  ret[5] = Value::uint(hwm);
  ret[6] = Value::uint(lwm);
  ret[7] = Value::uint(count);
  return ret;
}

EventAccumulatorByScopeTable* EventAccumulatorByScopeTable::create(std::string name, std::string oid)
{
  return new EventAccumulatorByScopeTableImpl(name, oid);
}

}
