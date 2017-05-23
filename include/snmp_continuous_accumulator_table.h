/**
 * @file snmp_continuous_accumulator_table.h
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
#include <atomic>

#include "logger.h"
#include "snmp_abstract_continuous_accumulator_table.h"

#ifndef SNMP_CONTINUOUS_ACCUMULATOR_TABLE_H
#define SNMP_CONTINUOUS_ACCUMULATOR_TABLE_H

// This file contains the interface for tables which:
//   - are indexed by time period
//   - accumulate data samples over time
//   - report a count of samples, high-water-mark and low-water-mark
//   - report mean sample rate and variance as weighted by the proportion
//         of time active within the time period
//   - carry across final value as initial value for the next table
//
// The thing sampled should be a continiuous data set, i.e. valued across a
// time period.
//
// To create an accumulator table, simply create one, and call `accumulate` on it as data comes in,
// e.g.:
//
// ContinuousAccumulatorTable* token_rate_table = ContinuousAccumulatorTable::create("token_rate", ".1.2.3");
// token_rate_table->accumulate(2000);

namespace SNMP
{

class ContinuousAccumulatorTable: public AbstractContinuousAccumulatorTable
{
public:
  virtual ~ContinuousAccumulatorTable() {};

  static ContinuousAccumulatorTable* create(std::string name, std::string oid);

  // Accumulate a sample into the underlying statistics.
  virtual void accumulate(uint32_t sample) = 0;

protected:
  ContinuousAccumulatorTable() {};

};

}

#endif
