/**
 * @file sip_event_priority.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef SIP_EVENT_PRIORITY_H
#define SIP_EVENT_PRIORITY_H

// Allowable priority levels for SIP events. Levels with higher values correspond
// to higher priorities.
enum SIPEventPriorityLevel
{
  NORMAL_PRIORITY=0,
  HIGH_PRIORITY_1,
  HIGH_PRIORITY_2,
  HIGH_PRIORITY_3,
  HIGH_PRIORITY_4,
  HIGH_PRIORITY_5,
  HIGH_PRIORITY_6,
  HIGH_PRIORITY_7,
  HIGH_PRIORITY_8,
  HIGH_PRIORITY_9,
  HIGH_PRIORITY_10,
  HIGH_PRIORITY_11,
  HIGH_PRIORITY_12,
  HIGH_PRIORITY_13,
  HIGH_PRIORITY_14,
  HIGH_PRIORITY_15
};

#endif
