/**
 * @file snmp_scalar_by_scope__table.h
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
#include "snmp_abstract_scalar.h"

#ifndef SNMP_SCALAR_BY_SCOPE_TABLE_H
#define SNMP_SCALAR_BY_SCOPE_TABLE_H

// This file contains the interface for tables which:
//   - are indexed by scope (node type)
//   - keep track of a single value
//   - report a single column for each scope value
//
// This is defined as an interface in order not to pollute the codebase with netsnmp include files
// (which indiscriminately #define things like READ and WRITE).

namespace SNMP
{
class ScalarByScopeTable: public AbstractScalar
{
public:
  static ScalarByScopeTable* create(std::string name, std::string oid);
  virtual void set_value(unsigned long value) = 0;
  virtual ~ScalarByScopeTable() {};

protected:
  ScalarByScopeTable() {};
};
}
#endif
