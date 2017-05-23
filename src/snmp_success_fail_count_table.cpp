/**
 * @file snmp_success_fail_count_table.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "snmp_internal/snmp_includes.h"
#include "snmp_internal/snmp_time_period_table.h"
#include "snmp_success_fail_count_table.h"
#include "snmp_statistics_structures.h"
#include "logger.h"

namespace SNMP
{

// Just a TimeBasedRow that maps the data from SuccessFailCount into the right columns.
class SuccessFailCountRow: public TimeBasedRow<SuccessFailCount>
{
public:
  SuccessFailCountRow(int index, View* view):
    TimeBasedRow<SuccessFailCount>(index, view) {};
  ColumnData get_columns()
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    SuccessFailCount* counts = _view->get_data(now);
    uint_fast32_t attempts = counts->attempts.load();
    uint_fast32_t successes = counts->successes.load();
    uint_fast32_t failures = counts->failures.load();
    uint_fast32_t success_percent_in_ten_thousands = 0;
    if (attempts == uint_fast32_t(0))
    {
      // If there are no attempts made we report the Success Percent as being
      // 100% to indicate that there have been no errors.
      // Note that units for Success Percent are actually 10,000's of a percent.
      success_percent_in_ten_thousands = 100 * 10000;
    }
    else if (successes > 0)
    {
      // Units for Success Percent are actually 10,000's of a percent.
      success_percent_in_ten_thousands = (successes * 100 * 10000) / (successes + failures);
    }

    // Construct and return a ColumnData with the appropriate values
    ColumnData ret;
    ret[1] = Value::integer(_index);
    ret[2] = Value::uint(attempts);
    ret[3] = Value::uint(successes);
    ret[4] = Value::uint(failures);
    ret[5] = Value::uint(success_percent_in_ten_thousands);
    return ret;
  }
};

class SuccessFailCountTableImpl: public ManagedTable<SuccessFailCountRow, int>, public SuccessFailCountTable
{
public:
  SuccessFailCountTableImpl(std::string name,
                            std::string tbl_oid):
    ManagedTable<SuccessFailCountRow, int>(name,
                                           tbl_oid,
                                           2,
                                           5, // Only columns 2-5 should be visible
                                           { ASN_INTEGER }), // Type of the index column
    five_second(5000),
    five_minute(300000)
  {
    // We have a fixed number of rows, so create them in the constructor.
    add(TimePeriodIndexes::scopePrevious5SecondPeriod);
    add(TimePeriodIndexes::scopeCurrent5MinutePeriod);
    add(TimePeriodIndexes::scopePrevious5MinutePeriod);
  }

  void increment_attempts()
  {
    // Increment each underlying set of data.
    five_second.get_current()->attempts++;
    five_minute.get_current()->attempts++;
  }

  void increment_successes()
  {
    // Increment each underlying set of data.
    five_second.get_current()->successes++;
    five_minute.get_current()->successes++;
  }

  void increment_failures()
  {
    // Increment each underlying set of data.
    five_second.get_current()->failures++;
    five_minute.get_current()->failures++;
  }

private:
  // Map row indexes to the view of the underlying data they should expose
  SuccessFailCountRow* new_row(int index)
  {
    SuccessFailCountRow::View* view = NULL;
    switch (index)
    {
      case TimePeriodIndexes::scopePrevious5SecondPeriod:
        view = new SuccessFailCountRow::PreviousView(&five_second);
        break;
      case TimePeriodIndexes::scopeCurrent5MinutePeriod:
        view = new SuccessFailCountRow::CurrentView(&five_minute);
        break;
      case TimePeriodIndexes::scopePrevious5MinutePeriod:
        view = new SuccessFailCountRow::PreviousView(&five_minute);
        break;
    }
    return new SuccessFailCountRow(index, view);
  }

  CurrentAndPrevious<SuccessFailCount> five_second;
  CurrentAndPrevious<SuccessFailCount> five_minute;
};

SuccessFailCountTable* SuccessFailCountTable::create(std::string name, std::string oid)
{
  return new SuccessFailCountTableImpl(name, oid);
}

}
