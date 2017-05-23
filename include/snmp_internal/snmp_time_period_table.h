/**
 * @file snmp_time_period_table.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
