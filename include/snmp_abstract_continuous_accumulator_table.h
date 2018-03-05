/**
 * @file snmp_abstract_continuous_accumulator_table.h
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

#ifndef SNMP_ABSTRACT_CONTINUOUS_ACCUMULATOR_TABLE_H
#define SNMP_ABSTRACT_CONTINUOUS_ACCUMULATOR_TABLE_H

// This file contains an abstract class used for creating tables Continuous
// Accumulator tables e.g. ContinuousAccumulatorTable,
// ContinuousAccumulatorByScopeTable.

namespace SNMP
{

class AbstractContinuousAccumulatorTable
{
public:
  AbstractContinuousAccumulatorTable() {};
  virtual ~AbstractContinuousAccumulatorTable() {};

  // Accumulate a sample into the underlying statistics.
  virtual void accumulate(uint32_t sample) = 0;

};

}

#endif
