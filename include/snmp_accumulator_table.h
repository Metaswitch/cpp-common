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
 * software and licensed under the OpenSSL Licenses.
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
#include "logger.h"

#ifndef SNMP_LATENCY_TABLE_H
#define SNMP_LATENCY_TABLE_H

namespace SNMP
{
// A group of rows for a latency or accumulator table, containing:
// - a row for the previous five seconds, with count of samples, average size, variance,
// low-water-mark and high-water-mark
// - a row for the previous five minutes, with count of samples, average size, variance,
// low-water-mark and high-water-mark
class AccumulatorRow : public Row
{
public:
  AccumulatorRow(int index_param, int interval_param) :
    Row(),
    index(index_param),
    interval(interval_param),
    tick(0)
  {
    accumulated = {0,};
    netsnmp_tdata_row_add_index(_row,
                                ASN_INTEGER,
                                &index,
                                sizeof(int));
    
  };
  ~AccumulatorRow() {};
  ColumnData get_columns();

  // Add a sample to the statistics
  void accumulate(uint32_t sample);

private:
  struct LatencyValues
  {
    uint64_t count;
    uint64_t sum;
    uint64_t sqsum;
    uint64_t hwm;
    uint64_t lwm;
  };

  uint32_t index;
  uint32_t interval;
  uint32_t tick;
  LatencyValues accumulated;

  // Make copy/move constructors private to avoid unexpected behaviour
  AccumulatorRow(const AccumulatorRow&);
  AccumulatorRow(const AccumulatorRow&&);

  void update_time();


};

class AccumulatorTable: public ManagedTable<AccumulatorRow, int>
{
public:
  AccumulatorTable(std::string name,
                   oid* tbl_oid,
                   int oidlen) :
    ManagedTable<AccumulatorRow, int>(name, tbl_oid, oidlen)
  {
    _tbl.add_index(ASN_INTEGER);
    _tbl.set_visible_columns(2, 6);

    add_row(0);
    add_row(1);
  }
  
  AccumulatorRow* new_row(int index)
  {
    switch (index)
    {
      case 0:
        // Five-second row
        return new AccumulatorRow(index, 5);
      case 1:
        // Five-minute row
        return new AccumulatorRow(index, 300);
      default:
        return NULL;
    }
  }

  void accumulate(uint32_t sample)
  {
    // Pass samples through to the underlying row group
    for (std::map<int, AccumulatorRow*>::iterator ii = _map.begin();
         ii != _map.end();
         ii++)
    {
      ii->second->accumulate(sample);
    }
  }

};

}

#endif
