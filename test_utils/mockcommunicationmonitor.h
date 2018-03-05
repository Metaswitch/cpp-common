/**
 * @file mockcommunicationmonitor.h Mock CommunicationMonitor.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCKCOMMUNICATIONMONITOR_H__
#define MOCKCOMMUNICATIONMONITOR_H__

#include "gmock/gmock.h"
#include "communicationmonitor.h"

class MockCommunicationMonitor : public CommunicationMonitor
{
public:
  MockCommunicationMonitor(AlarmManager* alarm_manager) :
    CommunicationMonitor(new Alarm(alarm_manager, "sprout", 0, AlarmDef::CRITICAL), "sprout", "chronos") {}

  MOCK_METHOD1(inform_success, void(unsigned long now_ms));
  MOCK_METHOD1(inform_failure, void(unsigned long now_ms));
};

#endif
