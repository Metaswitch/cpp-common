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

#ifndef SNMP_TIME_PERIOD_TABLE_H
#define SNMP_TIME_PERIOD_TABLE_H

namespace SNMP
{
namespace TimeData
{

template <class T> class CurrentAndPrevious
{
public:
  CurrentAndPrevious(int interval): _interval(interval), _tick(0) {}
  void update_time()
  {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &now);

    // The 'tick' signifies how many five-second windows have passed - if it's odd, we should read
    // from fiveseconds_odd and fiveseconds_even. If it's even, vice-versa.
    uint32_t new_tick = (now.tv_sec / _interval);

    if (new_tick > _tick)
    {
      if ((new_tick % 2) == 0)
      {
        current = &a;
        previous = &b;
      }
      else
      {
        current = &b;
        previous = &a;
      }
      (*current) = {0,};
    }
    _tick = new_tick;
  }

private:
  uint32_t _interval;
  uint32_t _tick;
  T a;
  T b;
  T* current;
  T* previous;
  
};

template <class T> class View
{
public:
  View(CurrentAndPrevious<T>* data): _data(data) {};
  virtual ~View() {};
  virtual T* get_data()
  {
    _data->update_time();
    return get_ptr();
  }
  virtual T* get_ptr() = 0;
  CurrentAndPrevious<T>* _data;
};

template <class T> class CurrentView : public View<T>
{
public:
  CurrentView(CurrentAndPrevious<T>* data): View<T>(data) {};

  T* get_ptr() { return this->_data->current; };
};

template <class T> class PreviousView : public View<T>
{
public:
  PreviousView(CurrentAndPrevious<T>* data): View<T>(data) {};
  T* get_ptr() { return this->_data->previous; };
};

template <class T> class TimeBasedRow : public Row
{
public:
  TimeBasedRow(int index, TimeData::View<T>* view) :
    Row(),
    _index(index),
    _view(view)
  {
    netsnmp_tdata_row_add_index(_row,
                                ASN_INTEGER,
                                &_index,
                                sizeof(int));
    
  };
  virtual ~TimeBasedRow()
  {
    delete(_view);
  };
  virtual ColumnData get_columns() = 0;

protected:
  uint32_t _index;
  View<T>* _view;
};

}
}

#endif
