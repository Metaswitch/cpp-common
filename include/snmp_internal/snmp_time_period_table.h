/**
 * @file snmp_time_period_table.h
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
#include "current_and_previous.h"
#include <vector>
#include <map>
#include <string>
#include <atomic>
#include "logger.h"

#include "snmp_types.h"

#ifndef SNMP_TIME_PERIOD_TABLE_H
#define SNMP_TIME_PERIOD_TABLE_H

// This file contains the base infrastructure for SNMP tables which are indexed by time period. It
// contains only abstract classes, which need to be subclassed - e.g. SNMP::AccumulatorRow and
// SNMP::EventAccumulatorTable.


namespace SNMP
{

template <class T> class TimeBasedRow : public Row
{
public:

  // A view into a CurrentAndPrevious set of data. See CurrentView and PreviousView.
  class View
  {
  public:
    View(CurrentAndPrevious<T>* data): _data(data) {};
    virtual ~View() {};
    virtual T* get_data(struct timespec now) = 0;
    // Return interval in ms
    uint32_t get_interval_ms() { return (this->_data->get_interval_ms()); }
  protected:
    CurrentAndPrevious<T>* _data;
  };

  // A view into the current part of a CurrentAndPrevious set of data.
  class CurrentView : public View
  {
  public:
    CurrentView(CurrentAndPrevious<T>* data): View(data) {};
    T* get_data(struct timespec now) { return this->_data->get_current(now); };
  };

  // A view into the previous part of a CurrentAndPrevious set of data.
  class PreviousView : public View
  {
  public:
    PreviousView(CurrentAndPrevious<T>* data): View(data) {};
    T* get_data(struct timespec now) { return this->_data->get_previous(now); };
  };


  // Finished with inner classes, back into TimeBasedRow.

  // Constructor, takes ownership of the View*.
  TimeBasedRow(int index, View* view) :
    Row(),
    _index(index),
    _view(view)
  {
    // Time-based rows are indexed off a single integer representing the time period.
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
  View* _view;
};
}

#endif
