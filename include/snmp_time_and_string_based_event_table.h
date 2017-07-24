/**
 * @file snmp_time_and_string_based_event_table.h
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <vector>
#include <map>

#ifndef SNMP_TIME_AND_STRING_BASED_EVENT_TABLE_H
#define SNMP_TIME_AND_STRING_BASED_EVENT_TABLE_H

// This file contains the interface for tables which:
//   - are indexed by time period and a string
//   - accumulate an event metric
//   - report columns for mean, variance, hwm, lwm and count.
//
// This is defined as an interface in order not to pollute the codebase with netsnmp include files
// (which indiscriminately #define things like READ and WRITE).

namespace SNMP
{

class TimeAndStringBasedEventTable
{
public:
  TimeAndStringBasedEventTable() {};
  virtual ~TimeAndStringBasedEventTable() {};

  static TimeAndStringBasedEventTable* create(std::string name, std::string oid);
  virtual void accumulate(std::string str_index, uint32_t sample) = 0;
};

}
#endif
