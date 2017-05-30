/**
 * @file snmp_time_period_and_double_indexed_table.h
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "snmp_time_period_table.h"

#ifndef SNMP_TIME_PERIOD_AND_DOUBLE_INDEXED_TABLE_H
#define SNMP_TIME_PERIOD_AND_DOUBLE_INDEXED_TABLE_H

// This file contains the base infrastructure for SNMP tables 
// which are indexed by time period and and two other values.
namespace SNMP
{

template <class T> class TimeAndDoubleIndexedRow : public TimeBasedRow<T>
{
public:
  // Constructor, takes ownership of the View*.
  TimeAndDoubleIndexedRow(int time_index, int first_index, int second_index, typename TimeBasedRow<T>::View* view) :
    TimeBasedRow<T>(time_index, view),
    _first_index(first_index),
    _second_index(second_index)
  {
    // Add first index (the time index is added in the base class).
    netsnmp_tdata_row_add_index(this->_row,
                                ASN_INTEGER,
                                &_first_index,
                                sizeof(int));
    // Add second index.
    netsnmp_tdata_row_add_index(this->_row,
                                ASN_INTEGER,
                                &_second_index,
                                sizeof(int));
  };

  virtual ~TimeAndDoubleIndexedRow()
  {
  };

protected:
  uint32_t _first_index;
  uint32_t _second_index;
};

}

#endif
