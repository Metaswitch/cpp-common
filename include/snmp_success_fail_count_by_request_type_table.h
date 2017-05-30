/**
 * @file snmp_success_fail_count_by_request_type_table.h
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
#include <string>
#include "snmp_sip_request_types.h"

#ifndef SNMP_SUCCESS_FAIL_COUNT_BY_REQUEST_TYPE_TABLE_H
#define SNMP_SUCCESS_FAIL_COUNT_BY_REQUEST_TYPE_TABLE_H

// This file contains the interface for tables which:
//   - are indexed by time period and SIP request type
//   - increment a count of the attempts, successes and failures over time
//   - report a count of the attempts, successes and failures
//
// This is defined as an interface in order not to pollute the codebase with netsnmp include files
// (which indiscriminately #define things like READ and WRITE).

namespace SNMP
{

class SuccessFailCountByRequestTypeTable
{
public:
  SuccessFailCountByRequestTypeTable() {};
  virtual ~SuccessFailCountByRequestTypeTable() {};

  static SuccessFailCountByRequestTypeTable* create(std::string name, std::string oid);
  virtual void increment_attempts(SIPRequestTypes request_type) = 0;
  virtual void increment_successes(SIPRequestTypes request_type) = 0;
  virtual void increment_failures(SIPRequestTypes request_type) = 0;
};

}
#endif
