/**
 * @file snmp_infinite_timer_count_table.cpp
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
