/**
 * @file mock_health_checker.hpp Mock health checker class for UT
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_HEALTH_CHECKER_HPP
#define MOCK_HEALTH_CHECKER_HPP

#include "gmock/gmock.h"
#include "health_checker.h"

class MockHealthChecker : public HealthChecker
{
public:
  MockHealthChecker() : HealthChecker() {}
  virtual ~MockHealthChecker() {}

  MOCK_METHOD0(health_check_passed, void());
};

#endif

