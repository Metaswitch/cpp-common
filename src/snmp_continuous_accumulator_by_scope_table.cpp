/**
 * @file snmp_continuous_accumulator_by_scope_table.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "snmp_statistics_structures.h"
#include "snmp_internal/snmp_time_period_table.h"
#include "snmp_internal/snmp_time_period_and_scope_table.h"
#include "snmp_continuous_accumulator_by_scope_table.h"
#include "limits.h"

namespace SNMP
{
// A TimeAndScopeBasedRow that maps the data from ContinuousStatistics into the right five columns.
class ContinuousAccumulatorByScopeRow: public TimeAndScopeBasedRow<ContinuousStatistics>
{
public:
  ContinuousAccumulatorByScopeRow(int index, std::string scope_index, View* view): TimeAndScopeBasedRow<ContinuousStatistics>(index, scope_index, view) {};
  ColumnData get_columns();
};

class ContinuousAccumulatorByScopeTableImpl: public ManagedTable<ContinuousAccumulatorByScopeRow, int>,
                                             public ContinuousAccumulatorByScopeTable
{
public:
  ContinuousAccumulatorByScopeTableImpl(std::string name,
                                        std::string tbl_oid):
                                        ManagedTable<ContinuousAccumulatorByScopeRow,int>
                                            (name,
                                            tbl_oid,
                                            3,
                                            7, // Columns 3-7 should be visible
                                            { ASN_INTEGER , ASN_OCTET_STR }), // Type of the index column
    five_second(5000),
    five_minute(300000)
  {
    n = 0;

    // We have a fixed number of rows, so create them in the constructor.
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
  ContinuousAccumulatorByScopeRow* new_row(int index)
  {
    ContinuousAccumulatorByScopeRow::View* view = NULL;
    switch (index)
    {
      case TimePeriodIndexes::scopePrevious5SecondPeriod:
        view = new ContinuousAccumulatorByScopeRow::PreviousView(&five_second);
        break;
      case TimePeriodIndexes::scopeCurrent5MinutePeriod:
        view = new ContinuousAccumulatorByScopeRow::CurrentView(&five_minute);
        break;
      case TimePeriodIndexes::scopePrevious5MinutePeriod:
        view = new ContinuousAccumulatorByScopeRow::PreviousView(&five_minute);
        break;
    }
    return new ContinuousAccumulatorByScopeRow(index, "node", view);
  }

  void accumulate_internal(CurrentAndPrevious<ContinuousStatistics>& data, uint32_t sample)
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    ContinuousStatistics* current_data = data.get_current(now);

    TRC_DEBUG("Accumulating sample %uui into continuous accumulator statistic", sample);

    current_data->count++;

    // Compute the updated sum and sqsum based on the previous values, dependent on
    // how long since an update happened. Additionally update the sum of squares as a
    // rolling total, and update the time of the last update. Also maintain a
    // current value held, that can be used if the period ends.

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

  int n;

  CurrentAndPrevious<ContinuousStatistics> five_second;
  CurrentAndPrevious<ContinuousStatistics> five_minute;
};

ColumnData ContinuousAccumulatorByScopeRow::get_columns()
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
  uint_fast64_t time_last_update_ms = accumulated->time_last_update_ms.load();
  uint_fast64_t time_period_start_ms = accumulated->time_period_start_ms.load();
  uint_fast64_t sum = accumulated->sum.load();
  uint_fast64_t sqsum = accumulated->sqsum.load();

  // We need to know how long it has been since we've last updated.
  // We must distinguish between the case that we have moved onto the
  // next period though, which we do by comparing the calculated end of period
  // with the current time. We should use the smaller value to accumulate with,
  // to stop us from updating periods that are now out of date.
  uint64_t time_now_ms = (now.tv_sec * 1000) + (now.tv_nsec / 1000000);

  // As a time period might not begin on a boundary, we must synchronize
  // the end of the period with a boundary which is why the following line
  // requires so many interval_ms!
  uint64_t time_period_end_ms = ((time_period_start_ms + interval_ms) / interval_ms) * interval_ms;
  uint64_t time_comes_first_ms = std::min(time_period_end_ms, time_now_ms);

  uint64_t time_since_last_update_ms = time_comes_first_ms - time_last_update_ms;
  accumulated->time_last_update_ms.store(time_comes_first_ms);
  uint64_t period_count = (time_comes_first_ms - time_period_start_ms);

  if (period_count > 0)
  {
    // Calculate the average and the variance from the stored average/time of last updated
    // and sum-of-squares.
    sum += time_since_last_update_ms * current_value;
    accumulated->sum.store(sum);
    sqsum += time_since_last_update_ms * current_value * current_value;
    accumulated->sqsum.store(sqsum);
    avg = sum / period_count;
    variance = ((sqsum * period_count) - (sum * sum)) / (period_count * period_count);
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

ContinuousAccumulatorByScopeTable* ContinuousAccumulatorByScopeTable::create(std::string name,
                                                                             std::string oid)
{
  return new ContinuousAccumulatorByScopeTableImpl(name, oid);
}
}
