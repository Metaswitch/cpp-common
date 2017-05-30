/**
 * @file snmp_infinite_scalar_table.h
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
#include <atomic>

#include "logger.h"

#ifndef SNMP_INFINITE_SCALAR_TABLE_H
#define SNMP_INFINITE_SCALAR_TABLE_H

namespace SNMP
{

class InfiniteScalarTable
{
public:
  InfiniteScalarTable() {};
  virtual ~InfiniteScalarTable() {};

  static InfiniteScalarTable* create(std::string name, std::string oid);

  virtual void increment(std::string, uint32_t) = 0;
  virtual void decrement(std::string, uint32_t) = 0;
};
}

#endif
