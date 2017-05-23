/**
 * @file snmp_continuous_increment_table.h
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

#ifndef SNMP_CONTINUOUS_INCREMENT_TABLE_H
#define SNMP_CONTINUOUS_INCREMENT_TABLE_H

// This file contains the interface for tables which:
//   - are indexed by time period
//   - adjust data by means of incrementing or decrementing the current running total
//   - report a count of samples, high-water-mark and low-water-mark
//   - report mean sample rate and variance as weighted by the proportion
//         of time active within the time period
//   - carry across final value as initial value for the next table
//
// The thing sampled should be a continiuous data set, i.e. valued across a
// time period.
//
// To create an increment table, simply create one, and call `increment` or 
// 'decrement' on it as data comes in,
// e.g.:
//
// ContinuousIncrementTable* token_rate_table = ContinuousIncrementTable::create("token_rate", ".1.2.3");
// token_rate_table->increment(1);

namespace SNMP
{
class ContinuousIncrementTable
{
public:
  ContinuousIncrementTable() {};
  virtual ~ContinuousIncrementTable() {};
  static ContinuousIncrementTable* create(std::string name, std::string oid);

  // Increment or decrement the underlying statistics.
  virtual void increment (uint32_t value) = 0;
  virtual void decrement (uint32_t value) = 0;
};
}
#endif
