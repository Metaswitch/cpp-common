/**
 * @file snmp_time_and_string_based_event_table.h
 *
 * Copyright (C) Metaswitch Networks 2017
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

// This file contains the interface for tables that:
//   - are indexed by time period and a string
//   - accumulate an event metric for different values of the string index
//   - report columns for mean, variance, hwm, lwm and count.
//
// An example would be a table that tracks SIP request latencies per
// application server URL.
//
// To create such a table, simply create one, and call `accumulate` on it as data comes in,
// e.g.:
//
// TimeAndStringBasedEventTable* as_latency_table = TimeAndStringBasedEventTable::create("per_as_sip_latencies", ".1.2.3");
// as_latency_table->accumulate("appserver.domain", 158);
//
// Rows are automatically added to the table the first time a measurement is
// accumulated for a given value of the string index.  They are never removed.
// It would be possible to enhance this table to support removal of string
// indices in future if that is required.
//
// This is defined as an interface in order not to pollute the codebase with netsnmp include files
// (which indiscriminately #define things like READ and WRITE).
//
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
