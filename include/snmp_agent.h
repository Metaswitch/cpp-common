/**
 * @file snmp_agent.h Initialization and termination functions for Sprout SNMP.
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>
#include "snmp_internal/snmp_includes.h"

#ifndef CW_SNMP_AGENT_H
#define CW_SNMP_AGENT_H

namespace SNMP
{

class Agent
{
public:
  // Create a new instance - not thread safe. Will delete any existing instances, which must no
  // longer be in use.
  static void instantiate(std::string name);

  // Destroy the instance - not thread safe. The instance must no longer be in use.
  static void deinstantiate();

  static inline Agent* instance() { return _instance; }

  void start(void);
  void stop(void);
  inline pthread_mutex_t& get_lock() { return _netsnmp_lock; }
  void add_row_to_table(netsnmp_tdata* table, netsnmp_tdata_row* row);
  void remove_row_from_table(netsnmp_tdata* table, netsnmp_tdata_row* row);

private:
  static Agent* _instance;
  std::string _name;
  pthread_t _thread;
  pthread_mutex_t _netsnmp_lock = PTHREAD_MUTEX_INITIALIZER;

  Agent(std::string name);
  ~Agent();

  static void* thread_fn(void* snmp_handler);
  void thread_fn(void);
  static int logging_callback(int majorID, int minorID, void* serverarg, void* clientarg);
};

}

// Starts the SNMP agent thread. 'name' is passed through to the netsnmp library as the application
// name - this is arbitrary, but should be spomething sensible (e.g. 'sprout', 'bono').
int snmp_setup(const char* name);

int init_snmp_handler_threads(const char* name);

// Terminates the SNMP agent thread. 'name' should match the string passed to snmp_setup.
void snmp_terminate(const char* name);

#endif
