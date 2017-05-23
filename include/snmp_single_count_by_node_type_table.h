/**
 * @file snmp_single_count_by_node_type_table.h
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
#include <string>
#include "snmp_node_types.h"

#ifndef SINGLE_COUNT_BY_NODE_TYPE_H
#define SINGLE_COUNT_BY_NODE_TYPE_H

// This file contains the interface for tables which:
//   - are indexed by time period and node type
//   - increment a single counter over time
//   - report a single column for each time period with that count
//
// This is defined as an interface in order not to pollute the codebase with netsnmp include files
// (which indiscriminately #define things like READ and WRITE).

namespace SNMP
{

class SingleCountByNodeTypeTable
{
public:
  SingleCountByNodeTypeTable() {};
  virtual ~SingleCountByNodeTypeTable() {};

  static SingleCountByNodeTypeTable* create(std::string name, std::string oid, std::vector<int> node_types);
  virtual void increment(NodeTypes node_type) = 0;
};

}
#endif
