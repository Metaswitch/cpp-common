/**
 * @file snmp_continuous_increment_table.cpp
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

#include "snmp_statistics_structures.h"
#include "snmp_internal/snmp_time_period_table.h"
#include "snmp_continuous_increment_table.h"
#include "limits.h"

namespace SNMP
{
// Just a TimeBasedRow that maps the data from ContinuousStatistics into the right five columns.
class ContinuousAccumulatorRow: public TimeBasedRow<ContinuousStatistics>
{
public:
  ContinuousAccumulatorRow(int index, View* view): TimeBasedRow<ContinuousStatistics>(index, view) {};
  ColumnData get_columns();
};

class ContinuousIncrementTableImpl: public ManagedTable<ContinuousAccumulatorRow, int>,
                                    public ContinuousIncrementTable
{
public:
  ContinuousIncrementTableImpl(std::string name,
                               std::string tbl_oid):
                               ManagedTable<ContinuousAccumulatorRow, int>
                                     (name,
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

  void increment(uint32_t value)
  {
    // Pass value as increment through to value adjusting structure.
    count_internal(five_second, value, TRUE);
    count_internal(five_minute, value, TRUE);
  }

  void decrement(uint32_t value)
  {
    // Pass value as decrement through to value adjusting structure.
    count_internal(five_second, value, FALSE);
    count_internal(five_minute, value, FALSE);
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

  void count_internal(CurrentAndPrevious<ContinuousStatistics>& data, uint32_t value_delta, bool increment_total)
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    ContinuousStatistics* current_data = data.get_current(now);

    uint64_t current_value = current_data->current_value;
    uint64_t new_value;
    // Attempt to calculate the new value, and set it to the atomic beneath.
    // If the atomic value has changed in the mean time, repeat the calculations
    // using the new current value.
    do
    {
      if (increment_total)
      {
        new_value = current_value + value_delta;
      }
      else
      {
        // Check to ensure the value to accumulate will not be negative,
        // and decrement, or leave value as default of 0.
        if (value_delta < current_value)
        {
          new_value = current_value - value_delta;
        }
        else
        {
          new_value = 0;
        }
      }
    } while (!current_data->current_value.compare_exchange_weak(current_value, new_value));

    accumulate_internal(current_data, current_value, new_value, now);
  }

  void accumulate_internal(ContinuousStatistics* current_data,
                           uint64_t current_value,
                           uint64_t sample,
                           const struct timespec& now)
  {
    current_data->count++;

    // Compute the updated sum and sqsum based on the previous values, dependent on
    // how long since an update happened. Additionally update the sum of squares as a
    // rolling total, and update the time of the last update. Also maintain a
    // current value held, that can be used if the period ends.
    uint64_t time_since_last_update = ((now.tv_sec * 1000) + (now.tv_nsec / 1000000))
                                     - (current_data->time_last_update_ms.load());

    current_data->time_last_update_ms = (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
    current_data->sum += current_value * time_since_last_update;
    current_data->sqsum += current_value * current_value * time_since_last_update;

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

  CurrentAndPrevious<ContinuousStatistics> five_second;
  CurrentAndPrevious<ContinuousStatistics> five_minute;
};

ColumnData ContinuousAccumulatorRow::get_columns()
{
  struct timespec now;
  clock_gettime(CLOCK_REALTIME_COARSE, &now);

  ContinuousStatistics* accumulated = _view->get_data(now);
  uint32_t interval_ms = _view->get_interval_ms();

  uint_fast32_t count = accumulated->count.load();
  uint_fast32_t current_value = accumulated->current_value.load();

  uint_fast64_t avg = current_value;
  uint_fast64_t variance = 0;
  uint_fast32_t lwm = accumulated->lwm.load();
  // If LWM is still ULONG_MAX, then report as 0, as no results
  // have been entered (and HWM will be reported as 0)
  if (lwm == ULONG_MAX)
  {
    lwm = 0;
  }
  uint_fast32_t hwm = accumulated->hwm.load();
  uint_fast64_t sum = accumulated->sum.load();
  uint_fast64_t sqsum = accumulated->sqsum.load();

  // We need to know how long it has been since we've last updated.
  // We must distinguish between the case that we have moved onto the
  // next period though, which we do by comparing the calculated end of period
  // with the current time. We should use the smaller value to accumulate with,
  // to stop us from updating periods that are now out of date.
  uint64_t time_now_ms = (now.tv_sec * 1000) + (now.tv_nsec / 1000000);


  // Get last update from the data structure, and set this update point
  // making sure we don't conflict with other threads
  uint64_t time_comes_first_ms;
  uint_fast64_t time_period_start_ms;
  uint64_t time_period_end_ms;
  uint_fast64_t time_last_update_ms = accumulated->time_last_update_ms.load();
  do
  {
    // Make sure that we have the correct period start time, in case we loop.
    time_period_start_ms = accumulated->time_period_start_ms.load();

    // As a time period might not begin on a boundary, we must synchronize
    // the end of the period with a boundary which is why the following line
    // requires so many interval_ms!
    time_period_end_ms = ((time_period_start_ms + interval_ms) / interval_ms) * interval_ms;

    // Choose the earlier of these as the new update point
    time_comes_first_ms = std::min(time_period_end_ms, time_now_ms);
  } while (!accumulated->time_last_update_ms.compare_exchange_weak(time_last_update_ms, time_comes_first_ms));

  uint64_t time_since_last_update_ms = time_comes_first_ms - time_last_update_ms;
  uint64_t period_count = (time_comes_first_ms - time_period_start_ms);

  if (period_count > 0)
  {
    // Calculate the average and the variance from the stored average/time of last updated
    // and sum-of-squares, and reinsert into the data safely.
    uint64_t new_sum;
    do
    {
      new_sum = sum + (time_since_last_update_ms * current_value);
    } while (!accumulated->sum.compare_exchange_weak(sum, new_sum));

    uint64_t new_sqsum;
    do
    {
      new_sqsum = sqsum + (time_since_last_update_ms * current_value * current_value);
    } while (!accumulated->sum.compare_exchange_weak(sqsum, new_sqsum));

    avg = new_sum / period_count;
    variance = ((new_sqsum * period_count) - (new_sum * new_sum)) / (period_count * period_count);
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

ContinuousIncrementTable* ContinuousIncrementTable::create(std::string name,
                                                           std::string oid)
{
  return new ContinuousIncrementTableImpl(name, oid);
}
}
