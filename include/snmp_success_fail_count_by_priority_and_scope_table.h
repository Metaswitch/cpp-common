/**
 * @file snmp_success_fail_count_by_priority_and_scope_table.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>

#ifndef SNMP_SUCCESS_FAIL_COUNT_BY_PRIORITY_AND_SCOPE_TABLE_H
#define SNMP_SUCCESS_FAIL_COUNT_BY_PRIORITY_AND_SCOPE_TABLE_H

// This file contains the interface for tables which:
//   - are indexed by time period, message priority and scope (node type)
//   - increment a count of the attempts, successes and failures over time
//   - report a count of the attempts, successes and failures
//
// This is defined as an interface in order not to pollute the codebase with netsnmp include files
// (which indiscriminately #define things like READ and WRITE).

namespace SNMP
{

class SuccessFailCountByPriorityAndScopeTable
{
public:
  SuccessFailCountByPriorityAndScopeTable() {};
  virtual ~SuccessFailCountByPriorityAndScopeTable() {};

  static SuccessFailCountByPriorityAndScopeTable* create(std::string name, std::string oid);
  virtual void increment_attempts(int priority) = 0;
  virtual void increment_successes(int priority) = 0;
  virtual void increment_failures(int priotity) = 0;
};

}
#endif
