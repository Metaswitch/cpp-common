/**
 * @file base_communication_monitor.cpp
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "base_communication_monitor.h"

BaseCommunicationMonitor::BaseCommunicationMonitor() :
  _succeeded(0),
  _failed(0)
{
  pthread_mutex_init(&_lock, NULL);
}

BaseCommunicationMonitor::~BaseCommunicationMonitor()
{
  pthread_mutex_destroy(&_lock);
}

void BaseCommunicationMonitor::inform_success(unsigned long now_ms)
{
  _succeeded++;
  track_communication_changes(now_ms);
}

void BaseCommunicationMonitor::inform_failure(unsigned long now_ms)
{
  _failed++;
  track_communication_changes(now_ms);
}
