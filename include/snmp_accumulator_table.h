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
#include "logger.h"

#ifndef SNMP_LATENCY_TABLE_H
#define SNMP_LATENCY_TABLE_H

namespace SNMP
{

class AccumulatedData
{
  AccumulatedData(int interval): _interval(interval), _tick(0), a(), b() {}
  struct Data
  {
    uint64_t count;
    uint64_t sum;
    uint64_t sqsum;
    uint64_t hwm;
    uint64_t lwm;
  };

  // Add a sample to the statistics
  void accumulate(uint32_t sample);
  uint32_t _interval;
  uint32_t _tick;
  Data a;
  Data b;
  Data* current;
  Data* previous;
  void update_time();
};

class AccumulatedDataView
{
  AccumulatedDataView(AccumulatedData* data): _data(data) {};
  virtual ~AccumulatedDataView() {};
  virtual AccumulatedData::Data* get_data() = 0;
  AccumulatedData* _data;
};

class CurrentAccumulatedDataView : public AccumulatedDataView
{
  CurrentAccumulatedDataView(AccumulatedData* data):
    AccumulatedDataView(data) {};
  AccumulatedData::Data* get_data()
  {
    _data->update_time();
    return _data->current;
  };
};

class PreviousAccumulatedDataView : public AccumulatedDataView
{
  PreviousAccumulatedDataView(AccumulatedData* data):
    AccumulatedDataView(data) {};
  AccumulatedData::Data* get_data()
  {
    _data->update_time();
    return _data->previous;
  };
};


// A group of rows for a latency or accumulator table, containing:
// - a row for the previous five seconds, with count of samples, average size, variance,
// low-water-mark and high-water-mark
// - a row for the previous five minutes, with count of samples, average size, variance,
// low-water-mark and high-water-mark
class AccumulatorRow : public Row
{
public:
  AccumulatorRow(int index_param, AccumulatedDataView* view_param) :
    Row(),
    index(index_param),
    _view(view_param)
  {
    netsnmp_tdata_row_add_index(_row,
                                ASN_INTEGER,
                                &index,
                                sizeof(int));
    
  };
  ~AccumulatorRow()
  {
    delete(_view);
  };
  ColumnData get_columns();


private:
  uint32_t index;
  AccumulatedDataView* _view;

  // Make copy/move constructors private to avoid unexpected behaviour
  AccumulatorRow(const AccumulatorRow&);
  AccumulatorRow(const AccumulatorRow&&);



};

class AccumulatorTable: public ManagedTable<AccumulatorRow, int>
{
public:
  AccumulatorTable(std::string name,
                   oid* tbl_oid,
                   int oidlen) :
    ManagedTable<AccumulatorRow, int>(name, tbl_oid, oidlen),
    five_second(5),
    five_minute(300)
  {
    _tbl.add_index(ASN_INTEGER);
    _tbl.set_visible_columns(2, 6);

    add_row(0);
    add_row(1);
//    add_row(2);
  }
  
  AccumulatorRow* new_row(int index)
  {
    switch (index)
    {
      case 0:
        // Five-second row
        return new AccumulatorRow(index, new PreviousAccumulatedDataView(&five_second));
      case 1:
        // Five-minute row
        return new AccumulatorRow(index, new CurrentAccumulatedDataView(&five_minute));
      case 2:
        // Five-minute row
        return new AccumulatorRow(index, new PreviousAccumulatedDataView(&five_minute));
      default:
        return NULL;
    }
  }

  void accumulate(uint32_t sample)
  {
    // Pass samples through to the underlying row group
    five_second.accumulate(sample);
    five_minute.accumulate(sample);
  }

  AccumulatedData five_second;
  AccumulatedData five_minute;
};

}

#endif
