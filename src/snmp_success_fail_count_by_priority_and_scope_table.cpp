/**
 * @file snmp_success_fail_count_by_priority_and_scope_table.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "snmp_success_fail_count_by_priority_and_scope_table.h"
#include "snmp_internal/snmp_includes.h"
#include "snmp_internal/snmp_counts_by_other_type_and_scope_table.h"
#include "snmp_statistics_structures.h"
#include "sip_event_priority.h"
#include "logger.h"

namespace SNMP
{

// Time, Priority and Scope Based Row that maps the data from SuccessFailCount into the right column.
class SuccessFailCountByPriorityAndScopeRow: public TimeOtherTypeAndScopeBasedRow<SuccessFailCount>
{
public:
  SuccessFailCountByPriorityAndScopeRow(int time_index, int type_index, View* view):
    TimeOtherTypeAndScopeBasedRow<SuccessFailCount>(time_index, type_index, "node", view) {};
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
    ret[4] = Value::uint(attempts);
    ret[5] = Value::uint(successes);
    ret[6] = Value::uint(failures);
    ret[7] = Value::uint(success_percent_in_ten_thousands);
    return ret;
  }
  static int get_count_size() { return 4; }
};

static std::vector<int> priorities =
{
  SIPEventPriorityLevel::NORMAL_PRIORITY,
  SIPEventPriorityLevel::HIGH_PRIORITY_1,
  SIPEventPriorityLevel::HIGH_PRIORITY_2,
  SIPEventPriorityLevel::HIGH_PRIORITY_3,
  SIPEventPriorityLevel::HIGH_PRIORITY_4,
  SIPEventPriorityLevel::HIGH_PRIORITY_5,
  SIPEventPriorityLevel::HIGH_PRIORITY_6,
  SIPEventPriorityLevel::HIGH_PRIORITY_7,
  SIPEventPriorityLevel::HIGH_PRIORITY_8,
  SIPEventPriorityLevel::HIGH_PRIORITY_9,
  SIPEventPriorityLevel::HIGH_PRIORITY_10,
  SIPEventPriorityLevel::HIGH_PRIORITY_11,
  SIPEventPriorityLevel::HIGH_PRIORITY_12,
  SIPEventPriorityLevel::HIGH_PRIORITY_13,
  SIPEventPriorityLevel::HIGH_PRIORITY_14,
  SIPEventPriorityLevel::HIGH_PRIORITY_15
};

class SuccessFailCountByPriorityAndScopeTableImpl: public CountsByOtherTypeAndScopeTableImpl<SuccessFailCountByPriorityAndScopeRow, SuccessFailCount>,
  public SuccessFailCountByPriorityAndScopeTable
{
public:
  SuccessFailCountByPriorityAndScopeTableImpl(std::string name,
                                              std::string tbl_oid):
    CountsByOtherTypeAndScopeTableImpl<SuccessFailCountByPriorityAndScopeRow,
                                       SuccessFailCount>(name,
                                                         tbl_oid,
                                                         priorities)
  {}

  void increment_attempts(int priority)
  {
    five_second[priority]->get_current()->attempts++;
    five_minute[priority]->get_current()->attempts++;
  }

  void increment_successes(int priority)
  {
    five_second[priority]->get_current()->successes++;
    five_minute[priority]->get_current()->successes++;
  }

  void increment_failures(int priority)
  {
    five_second[priority]->get_current()->failures++;
    five_minute[priority]->get_current()->failures++;
  }
};

SuccessFailCountByPriorityAndScopeTable* SuccessFailCountByPriorityAndScopeTable::create(std::string name,
                                                                                         std::string oid)
{
  return new SuccessFailCountByPriorityAndScopeTableImpl(name, oid);
}

}
