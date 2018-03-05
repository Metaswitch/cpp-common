/**
 * @file snmp_cx_counter_table.h
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

#ifndef CX_COUNTER_H
#define CX_COUNTER_H

// This file contains the interface for tables which:
//   - are indexed by time period, App ID and result-code
//   - increment a single counter over time
//   - report a single column for each time period with that count
//
// This is defined as an interface in order not to pollute the codebase with netsnmp include files
// (which indiscriminately #define things like READ and WRITE).

namespace SNMP
{

enum DiameterAppId
{
  BASE = 0,
  _3GPP = 1,
  TIMEOUT = 2
};

class CxCounterTable
{
public:
  CxCounterTable() {};
  virtual ~CxCounterTable() {};

  static CxCounterTable* create(std::string name, std::string oid);
  virtual void increment(DiameterAppId appId, int result_code) = 0;
};

}
#endif
