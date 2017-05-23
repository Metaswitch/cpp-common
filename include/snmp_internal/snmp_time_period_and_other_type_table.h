/**
 * @file snmp_time_period_and_other_type_table.h
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

#ifndef SNMP_TIME_PERIOD_AND_OTHER_TYPE_TABLE_H
#define SNMP_TIME_PERIOD_AND_OTHER_TYPE_TABLE_H

// This file contains the base infrastructure for SNMP tables 
// which are indexed by time period and another type (e.g. node, SIP request
// method).
namespace SNMP
{

template <class T> class TimeAndOtherTypeBasedRow : public TimeBasedRow<T>
{
public:
  // Constructor, takes ownership of the View*.
  TimeAndOtherTypeBasedRow(int time_index, int type_index, typename TimeBasedRow<T>::View* view) :
    TimeBasedRow<T>(time_index, view),
    _type_index(type_index)
  {
    // Add index for the other type (the time index is added in the base class)
    netsnmp_tdata_row_add_index(this->_row,
                                ASN_INTEGER,
                                &_type_index,
                                sizeof(int));
  };

  virtual ~TimeAndOtherTypeBasedRow()
  {
  };

protected:
  uint32_t _type_index;
};

}

#endif
