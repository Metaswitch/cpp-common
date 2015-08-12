/**
 * @file snmp_continuous_accumulator_table.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2015 Metaswitch Networks Ltd
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
#include "snmp_continuous_accumulator_table.h"
#include "limits.h"

namespace SNMP
{

// Storage for the underlying data
struct ContinuousStatistics
{
  std::atomic_uint_fast64_t count;
  std::atomic_uint_fast64_t current_value;
  std::atomic_uint_fast64_t time_last_update_ms;
  std::atomic_uint_fast64_t time_period_start_ms;
  std::atomic_uint_fast64_t sum;
  std::atomic_uint_fast64_t sqsum;
  std::atomic_uint_fast64_t hwm;
  std::atomic_uint_fast64_t lwm;

  void reset(uint64_t periodstart, ContinuousStatistics* previous = NULL);
};

// Just a TimeBasedRow that maps the data from ContinuousStatistics into the right five columns.
class ContinuousAccumulatorRow: public TimeBasedRow<ContinuousStatistics>
{
public:
  ContinuousAccumulatorRow(int index, View* view): TimeBasedRow<ContinuousStatistics>(index, view) {};
  ColumnData get_columns();
};

class ContinuousAccumulatorTableImpl: public ManagedTable<ContinuousAccumulatorRow, int>, public ContinuousAccumulatorTable
{
public:
  ContinuousAccumulatorTableImpl(std::string name,
                       std::string tbl_oid):
    ManagedTable<ContinuousAccumulatorRow, int>(name,
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
  ContinuousAccumulatorRow* new_row(int index)
  {
    ContinuousAccumulatorRow::View* view = NULL;
    switch (index)
    {
      case TimePeriodIndexes::scopePrevious5SecondPeriod:
        view = new ContinuousAccumulatorRow::PreviousView(&five_second);
        break;
      case TimePeriodIndexes::scopeCurrent5MinutePeriod:
        view = new ContinuousAccumulatorRow::CurrentView(&five_minute);
        break;
      case TimePeriodIndexes::scopePrevious5MinutePeriod:
        view = new ContinuousAccumulatorRow::PreviousView(&five_minute);
        break;
    }
    return new ContinuousAccumulatorRow(index, view);
  }

  void accumulate_internal(ContinuousAccumulatorRow::CurrentAndPrevious& data, uint32_t sample)
  {
    ContinuousStatistics* current_data = data.get_current();

    TRC_DEBUG("Accumulating sample %uui into continuous accumulator statistic", sample);

    current_data->count++;

    // Compute the updated sum and sqsum based on the previous values, dependent on
    // how long since an update happened. Additionally update the sum of squares as a
    // rolling total, and update the time of the last update. Also maintain a
    // current value held, that can be used if the period ends.
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    uint64_t time_since_last_update = ((now.tv_sec * 1000) + (now.tv_nsec / 1000000))
                                     - (current_data->time_last_update_ms.load());
    uint32_t current_value = current_data->current_value.load();

    current_data->time_last_update_ms = (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
    current_data->sum += current_value * time_since_last_update;
    current_data->sqsum += current_value * current_value * time_since_last_update;
    current_data->current_value = sample;

    // Update the low- and high-water marks.  In each case, we get the current
    // value, decide whether a change is required and then atomically swap it
    // if so, repeating if it was changed in the meantime.  Note that
    // compare_exchange_weak loads the current value into the expected value
    // parameter (lwm or hwm below) if the compare fails.
    uint_fast64_t lwm = current_data->lwm.load();
    while ((sample < lwm) &&
           (!current_data->lwm.compare_exchange_weak(lwm, sample)))
    {
      // Do nothing.
    }
    uint_fast64_t hwm = current_data->hwm.load();
    while ((sample > hwm) &&
           (!current_data->hwm.compare_exchange_weak(hwm, sample)))
    {
      // Do nothing.
    }
  };


  ContinuousAccumulatorRow::CurrentAndPrevious five_second;
  ContinuousAccumulatorRow::CurrentAndPrevious five_minute;
};

// Reset the table in preparation for a new time period
// Statistics can be carried over from the previous table
void ContinuousStatistics::reset(uint64_t periodstart_ms, ContinuousStatistics* previous)
{
  struct timespec now;
  clock_gettime(CLOCK_REALTIME_COARSE, &now);

  // At time 0, all incrementing values should be 0
  count.store(0);
  sum.store(0);
  sqsum.store(0);

  // Carry across the previous values from the last table,
  // allowing us to set current, lwm and hwm.
  if (previous != NULL)
  {
    current_value.store(previous->current_value.load());
    lwm.store(previous->current_value.load());
    hwm.store(previous->current_value.load());
  }
  // Without any new data, default the values to 0
  else
  {
    current_value.store(0);
    lwm.store(0);
    hwm.store(0);
  }

  // Given a ridiculuous periodstart, default the value
  // to the current time
  if (periodstart_ms == 0)
  {
    uint64_t time_now_ms = (now.tv_sec * 1000) + (now.tv_nsec / 1000000);

    time_last_update_ms.store(time_now_ms);
    time_period_start_ms.store(time_now_ms);
  }
  // Set the last update time to be the start of the period
  // Letting us calculate the incrementing values more accurately in
  // accumulate() or get_columns()
  // (As they were set to 0 above)
  else
  {
    time_last_update_ms.store(periodstart_ms);
    time_period_start_ms.store(periodstart_ms);
  }
}

ColumnData ContinuousAccumulatorRow::get_columns()
{
  ContinuousStatistics* accumulated = _view->get_data();
  uint32_t interval_ms = _view->get_interval_ms();

  uint_fast32_t count = accumulated->count.load();
  uint_fast32_t current_value = accumulated->current_value.load();

  uint_fast64_t avg = current_value;
  uint_fast64_t variance = 0;
  uint_fast32_t lwm = accumulated->lwm.load();
  uint_fast32_t hwm = accumulated->hwm.load();
  uint_fast64_t time_last_update_ms = accumulated->time_last_update_ms.load();
  uint_fast64_t time_period_start_ms = accumulated->time_period_start_ms.load();
  uint_fast64_t sum = accumulated->sum.load();
  uint_fast64_t sqsum = accumulated->sqsum.load();

  // We need to know how long it has been since we've last updated.
  // We must distinguish between the case that we have moved onto the
  // next period though, which we do by comparing the calculated end of period
  // with the current time. We should use the smaller value to accumulate with,
  // to stop us from updating periods that are now out of date.
  struct timespec now;
  clock_gettime(CLOCK_REALTIME_COARSE, &now);
  uint64_t time_now_ms = (now.tv_sec * 1000) + (now.tv_nsec / 1000000);

  // As a time period might not begin on a boundary, we must synchronize
  // the end of the period with a boundary which is why the following line
  // requires so many interval_ms!
  uint64_t time_period_end_ms = ((time_period_start_ms + interval_ms) / interval_ms) * interval_ms;
  uint64_t time_comes_first_ms = std::min(time_period_end_ms, time_now_ms);

  uint64_t time_since_last_update_ms = time_comes_first_ms - time_last_update_ms;
  accumulated->time_last_update_ms.store(time_comes_first_ms);

  if (time_since_last_update_ms > 0)
  {
    // Calculate the average and the variance from the stored average/time of last upadted
    // and sum-of-squares.
    sum += time_since_last_update_ms * current_value;
    accumulated->sum.store(sum);
    sqsum += time_since_last_update_ms * current_value * current_value;
    accumulated->sqsum.store(sqsum);
    avg = sum/(time_comes_first_ms - time_period_start_ms);
    variance = sqsum/(time_comes_first_ms - time_period_start_ms) - (avg * avg);
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

ContinuousAccumulatorTable* ContinuousAccumulatorTable::create(std::string name, std::string oid)
{
  return new ContinuousAccumulatorTableImpl(name, oid);

}
}
