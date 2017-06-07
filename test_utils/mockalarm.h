/**
 * @file mockalarms.h 
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCKALARM_H__
#define MOCKALARM_H__

#include "gmock/gmock.h"
#include "alarm.h"

class MockAlarm : public Alarm
{
public:
  MockAlarm(AlarmManager* alarm_manager) :
    Alarm(alarm_manager, "sprout", 0) {}

  MOCK_METHOD0(clear, void());
  MOCK_METHOD0(get_alarm_state, AlarmState::AlarmCondition());
  MOCK_METHOD1(set, void(AlarmDef::Severity));
};

#endif
