/**
 * @file snmp_time_period_and_string_table.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "snmp_time_period_table.h"
#include "snmp_node_types.h"

#ifndef SNMP_TIME_PERIOD_AND_STRING_TABLE_H
#define SNMP_TIME_PERIOD_AND_STRING_TABLE_H

// This file contains the base infrastructure for SNMP tables
// which are indexed by time period and a string (e.g. app server URI.
namespace SNMP
{

template <class T> class TimeAndStringBasedRow : public TimeBasedRow<T>
{
public:
  // Constructor, takes ownership of the View*.
  TimeAndStringBasedRow(int time_index, std::string string_index, typename TimeBasedRow<T>::View* view) :
    TimeBasedRow<T>(time_index, view),
    _string_index(string_index)
  {
    // Add the string index (the time index is added in the base class)
    netsnmp_tdata_row_add_index(this->_row,
                                ASN_OCTET_STR,
                                string_index.c_str(),
                                string_index.length());
  };

  virtual ~TimeAndStringBasedRow()
  {
  };

protected:
  std::string _string_index;
};

}

#endif
