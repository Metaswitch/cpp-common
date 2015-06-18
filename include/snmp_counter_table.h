/**
 * @file snmp_latency_table.h
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
 * software and licensed und er the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include "snmp_table.h"
#include <vector>
#include <map>
#include <string>
#include <tuple>
#include "snmp_includes.h"
#include "snmp_time_period_table.h"
#include "logger.h"

#ifndef SNMP_COUNTER_TABLE_H
#define SNMP_COUNTER_TABLE_H

namespace SNMP
{
struct SingleCount
{
  uint64_t count;
};

class CounterData: public TimeBasedRow<SingleCount>::CurrentAndPrevious
{
  CounterData(int interval):
    TimeBasedRow<SingleCount>::CurrentAndPrevious(interval)
  {
    a = {0};
    b = {0};
  };
  
  // Add a sample to the statistics
  void increment() { this->current->count++; };
};

class CounterRow: public TimeBasedRow<SingleCount>
{
public:
  CounterRow(int index, View* view):
    TimeBasedRow<SingleCount>(index, view) {};
  ColumnData get_columns();
};

class CounterTable: public ManagedTable<CounterRow, int>
{
public:
  CounterTable(std::string name,
                   oid* tbl_oid,
                   int oidlen) :
    ManagedTable<CounterRow, int>(name, tbl_oid, oidlen),
    five_second(5),
    five_minute(300)
  {
    _tbl.add_index(ASN_INTEGER);
    _tbl.set_visible_columns(2, 3);

    add_row(0);
    add_row(1);
    add_row(2);
  }
  
  CounterRow* new_row(int index)
  {
    CounterRow::View* view = NULL;
    switch (index)
    {
      case 0:
        // Five-second row
        view = new CounterRow::PreviousView(&five_second);
        break;
      case 1:
        // Five-minute row
        view = new CounterRow::CurrentView(&five_minute);
        break;
      case 2:
        // Five-minute row
        view = new CounterRow::PreviousView(&five_minute);
        break;
    }
    return new CounterRow(index, view);
  }

  void increment()
  {
    // Pass samples through to the underlying row group
    five_second.increment();
    five_minute.increment();
  }

  CounterData five_second;
  CounterData five_minute;
};

}

#endif
