/**
 * @file snmp_infinite_timer_count_table.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>
#include <algorithm>
#include <memory>

#include "snmp_statistics_structures.h"
#include "snmp_infinite_timer_count_table.h"
#include "timer_counter.h"
#include "snmp_row.h"
#include "snmp_infinite_base_table.h"

#include "log.h"
#include "logger.h"

namespace SNMP
{
  class InfiniteTimerCountTableImpl : public InfiniteTimerCountTable, public InfiniteBaseTable
  {
  public:
    InfiniteTimerCountTableImpl(std::string name, // Name of this table, for logging
                                std::string tbl_oid) : // Root OID of this table
                                InfiniteBaseTable(name, tbl_oid, max_row, max_column){}

    virtual ~InfiniteTimerCountTableImpl(){};
    
    void increment(std::string tag, uint32_t count)
    {
      _timer_counters[tag].increment(count);
    }

    void decrement(std::string tag, uint32_t count)
    {
      _timer_counters[tag].decrement(count);
    }

  protected:
    static const uint32_t max_row = 3;
    static const uint32_t max_column = 5;
    std::map<std::string, TimerCounter> _timer_counters;

  private:
    Value get_value(std::string tag,
                    uint32_t column,
                    uint32_t row,
                    timespec now)
    {
      SimpleStatistics stats;
      Value result = Value::uint(0);
      // Update and obtain the relevants statistics structure
      _timer_counters[tag].get_statistics(row, now, &stats);

      // Calculate the appropriate value - i.e. avg, var, hwm or lwm
      result = read_column(&stats, tag, column, now);
      TRC_DEBUG("Got value %u for tag %s cell (%d, %d)",
                *result.value, tag.c_str(), row, column);

      return result;
    }

    Value read_column(SimpleStatistics* data,
                       std::string tag,
                       uint32_t column,
                       timespec now)
    {
      switch (column)
      {
        case 2:
          // Calculate the average
          return Value::uint(data->average);
        case 3:
          // Calculate the variance
          return Value::uint(data->variance);
        case 4:
          // Get the HWM
          return Value::uint(data->hwm);
        case 5:
          // Get the LWM
          return Value::uint(data->lwm);
        default:
          // This should never happen - find_next_oid should police this.
          TRC_DEBUG("Internal MIB error - column %d is out of bounds",
                    column);
          return Value::uint(0);
      }
    }

  };

  InfiniteTimerCountTable* InfiniteTimerCountTable::create(std::string name, std::string oid)
  {
    return new InfiniteTimerCountTableImpl(name, oid);
  };
}
