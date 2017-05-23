/**
 * @file snmp_event_accumulator_table.h
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

#ifndef SNMP_EVENT_ACCUMULATOR_TABLE_H
#define SNMP_EVENT_ACCUMULATOR_TABLE_H

// This file contains the interface for tables which:
//   - are indexed by time period
//   - accumulate data samples over time
//   - report a count of samples, mean sample value, variance, high-water-mark and low-water-mark
//   - reset completely at the end of the period
//
// The thing sampled should be event related, i.e. size of a queue, latency
// values
//
// To create an event accumulator table, simply create one, and call `accumulate` on it as data comes in,
// e.g.:
//
// EventAccumulatorTable* bono_latency_table = EventAccumulatorTable::create("bono_latency", ".1.2.3");
// bono_latency_table->accumulate(2000);

namespace SNMP
{

class EventAccumulatorTable
{
public:
  virtual ~EventAccumulatorTable() {};

  static EventAccumulatorTable* create(std::string name, std::string oid);

  // Accumulate a sample into the underlying statistics.
  virtual void accumulate(uint32_t sample) = 0;

protected:
  EventAccumulatorTable() {};

};

}

#endif
