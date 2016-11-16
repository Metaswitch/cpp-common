/**
 * @file snmp_counter_by_scope_table.cpp
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

#include "snmp_counter_table.h"
#include "snmp_counter_by_scope_table.h"
#include "snmp_statistics_structures.h"
#include "snmp_internal/snmp_includes.h"
#include "snmp_internal/snmp_time_period_table.h"
#include "snmp_internal/snmp_time_period_and_scope_table.h"
#include "logger.h"

namespace SNMP
{

// A TimeAndScopeBasedRow that maps the data from SingleCount into the right column.
class CounterByScopeRow: public TimeAndScopeBasedRow<SingleCount>
{
public:
  CounterByScopeRow(int index, std::string scope_index, View* view):
    TimeAndScopeBasedRow<SingleCount>(index, scope_index, view) {};
  ColumnData get_columns()
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    SingleCount accumulated = *(this->_view->get_data(now));

    // Construct and return a ColumnData with the appropriate values
    ColumnData ret;
    ret[3] = Value::uint(accumulated.count);
    return ret;
  }
};

class CounterTableByScopeImpl: public ManagedTable<CounterByScopeRow, int>, public CounterByScopeTable
{
public:
  CounterTableByScopeImpl(std::string name,
                          std::string tbl_oid):
    ManagedTable<CounterByScopeRow, int>(name,
                                         tbl_oid,
                                         3,
                                         3, // Only column 3 should be visible
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

  void increment()
  {
    // Increment each underlying set of data.
    five_second.get_current()->count++;
    five_minute.get_current()->count++;
  }

private:
  // Map row indexes to the view of the underlying data they should expose
  CounterByScopeRow* new_row(int index)
  {
    CounterByScopeRow::View* view = NULL;
    switch (index)
    {
      case TimePeriodIndexes::scopePrevious5SecondPeriod:
        view = new CounterByScopeRow::PreviousView(&five_second);
        break;
      case TimePeriodIndexes::scopeCurrent5MinutePeriod:
        view = new CounterByScopeRow::CurrentView(&five_minute);
        break;
      case TimePeriodIndexes::scopePrevious5MinutePeriod:
        view = new CounterByScopeRow::PreviousView(&five_minute);
        break;
    }
    return new CounterByScopeRow(index, "node", view);
  }

  int n;

  CurrentAndPrevious<SingleCount> five_second;
  CurrentAndPrevious<SingleCount> five_minute;
};

CounterByScopeTable* CounterByScopeTable::create(std::string name, std::string oid)
{
  return new CounterTableByScopeImpl(name, oid);
}

}
