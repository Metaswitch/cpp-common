/**
 * @file mockloadmonitor.hpp Mock load monitor class for UT
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCKLOADMONITOR_HPP__
#define MOCKLOADMONITOR_HPP__

#include "gmock/gmock.h"
#include "load_monitor.h"

class MockLoadMonitor : public LoadMonitor
{
public:
  MockLoadMonitor() : LoadMonitor(0, 0, 0.0, 0.0, 0.0) {}
  virtual ~MockLoadMonitor() {}

  MOCK_METHOD2(admit_request, bool(SAS::TrailId id, bool admit_anyway));
  MOCK_METHOD0(incr_penalties, void());
  MOCK_METHOD2(request_complete, void(uint64_t latency,
                                      SAS::TrailId id));
  MOCK_METHOD0(get_target_latency_us, int());
  MOCK_METHOD0(update_statistics, void());
};

#endif

