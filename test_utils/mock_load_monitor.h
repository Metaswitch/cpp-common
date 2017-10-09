/**
 * @file mock_load_monitor.h Mock LoadMonitor
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_LOAD_MONITOR_H__
#define MOCK_LOAD_MONITOR_H__

#include "gmock/gmock.h"
#include "load_monitor.h"

class MockLoadMonitor : public LoadMonitor
{
  MockLoadMonitor() : LoadMonitor(0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL) {}
  ~MockLoadMonitor() {}

  MOCK_METHOD1(admit_request, bool(SAS::TrailId trail));
  MOCK_METHOD0(incr_penalties, void());
  MOCK_METHOD0(get_target_latency_us, int());
  MOCK_METHOD1(request_complete, void(int latency));
  MOCK_METHOD0(update_statistics, void());

  MOCK_METHOD0(get_target_latency, int());
  MOCK_METHOD0(get_current_latency, int());
  MOCK_METHOD0(get_rate_limit, float());

#endif
