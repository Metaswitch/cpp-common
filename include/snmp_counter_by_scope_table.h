/**
 * @file snmp_counter_by_scope_table.h
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <vector>
#include <map>
#include <string>

#ifndef SNMP_COUNTER_BY_SCOPE_TABLE_H
#define SNMP_COUNTER_BY_SCOPE_TABLE_H

// This file contains the interface for tables which:
//   - are indexed by time period and scope (node type)
//   - increment a single counter over time
//   - report a single column for each time period with that count
//
// This is defined as an interface in order not to pollute the codebase with netsnmp include files
// (which indiscriminately #define things like READ and WRITE).

namespace SNMP
{
class CounterByScopeTable
{
public:
  static CounterByScopeTable* create(std::string name, std::string oid);
  virtual void increment() = 0;
  virtual ~CounterByScopeTable() {};

protected:
  CounterByScopeTable() {};
};
}
#endif
