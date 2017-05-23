/**
 * @file base_communication_monitor.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef BASE_COMMUNICATION_MONITOR_H__
#define BASE_COMMUNICATION_MONITOR_H__

#include <pthread.h>
#include <string>
#include <atomic>

/// @class CommunicationMonitor
///
/// Abstract class that provides a simple mechanism to track communication 
/// state for an entity.
///
///   - whenever an entity successfully communicates with a peer, the
///     inform_success() method should be called
///
///   - whenever an entity fails to communicate with a peer, the 
///     inform_failure() method should be called
class BaseCommunicationMonitor
{
public:
  virtual ~BaseCommunicationMonitor();

  /// Report a successful communication. If the current time in ms is available
  /// to the caller it should be passed to avoid duplicate work.
  virtual void inform_success(unsigned long now_ms = 0);

  /// Report a failed communication. If the current time in ms is available to
  /// the caller it should be passed to avoid duplicate work.
  virtual void inform_failure(unsigned long now_ms = 0);

protected:
  BaseCommunicationMonitor();

  /// Carry out any desired behaviour given the current communication state
  /// (implemented by subclass). 
  virtual void track_communication_changes(unsigned long now_ms = 0) = 0;

  std::atomic<int> _succeeded;
  std::atomic<int> _failed;
  pthread_mutex_t _lock;
};

#endif
