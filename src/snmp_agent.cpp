/**
 * @file snmp_agent.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <limits.h>
#include <net-snmp/library/large_fd_set.h>
#include "snmp_internal/snmp_includes.h"
#include "snmp_agent.h"
#include "log.h"

namespace SNMP
{

Agent* Agent::_instance = NULL;

void Agent::instantiate(std::string name)
{
  delete(_instance);
  _instance = NULL;
  _instance = new Agent(name);
}

void Agent::deinstantiate()
{
  delete(_instance);
  _instance = NULL;
}

Agent::Agent(std::string name) : _name(name)
{
  pthread_mutex_lock(&_netsnmp_lock);

  // Make sure we start as a subagent, not a master agent.
  netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 1);

  // Set the persistent directory to somewhere that the process can write to
  std::string persistent_file = "/tmp/";
  persistent_file.append(_name.c_str());
  netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID,
                        NETSNMP_DS_LIB_PERSISTENT_DIR,
                        persistent_file.c_str());

  // Uncomment this line to send AgentX requests over TCP, rather than a unix
  // domain socket, in order to snoop them with tcpdump. You'll also need to
  // replace the agentXSocket line in /etc/snmp/snmpd.conf on your node with
  // 'agentXSocket tcp:localhost:705' and restart snmpd.
  // netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_X_SOCKET, "tcp:localhost:705");

  // Use callback-based logging, and integrate it with the Clearwater logger
  snmp_enable_calllog();
  snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_LOGGING, logging_callback, NULL);

  netsnmp_container_init_list();
  int rc = init_agent(_name.c_str());
  if (rc != 0)
  {
    snmp_unregister_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_LOGGING, logging_callback, NULL, 1);
    netsnmp_container_free_list();
  }

  pthread_mutex_unlock(&_netsnmp_lock);

  if (rc != 0)
  {
    throw rc;
  }
}

Agent::~Agent()
{
  snmp_unregister_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_LOGGING, logging_callback, NULL, 1);
  netsnmp_container_free_list();
}

void Agent::start(void)
{
  pthread_mutex_lock(&_netsnmp_lock);
  init_snmp(_name.c_str());
  pthread_mutex_unlock(&_netsnmp_lock);

  int rc = pthread_create(&_thread, NULL, thread_fn, this);
  if (rc != 0)
  {
    throw rc;
  }
}

void Agent::stop(void)
{
  pthread_cancel(_thread);
  pthread_join(_thread, NULL);

  pthread_mutex_lock(&_netsnmp_lock);
  snmp_shutdown(_name.c_str());
  netsnmp_container_free_list();
  pthread_mutex_unlock(&_netsnmp_lock);
}

void Agent::add_row_to_table(netsnmp_tdata* table, netsnmp_tdata_row* row)
{
  pthread_mutex_lock(&_netsnmp_lock);
  netsnmp_tdata_add_row(table, row);
  pthread_mutex_unlock(&_netsnmp_lock);
}

void Agent::remove_row_from_table(netsnmp_tdata* table, netsnmp_tdata_row* row)
{
  pthread_mutex_lock(&_netsnmp_lock);
  netsnmp_tdata_remove_row(table, row);
  pthread_mutex_unlock(&_netsnmp_lock);
}

void* Agent::thread_fn(void* snmp_agent)
{
  ((Agent*)snmp_agent)->thread_fn();
  return NULL;
}

void Agent::thread_fn()
{
  int num_fds;
  netsnmp_large_fd_set read_fds;
  netsnmp_large_fd_set_init(&read_fds, FD_SETSIZE);
  struct timeval timeout;
  int block;

  while (1)
  {
    // Set up some variables and call into Net-SNMP to initialize them ready
    // for the select call.
    num_fds = 0;
    NETSNMP_LARGE_FD_ZERO(&read_fds);
    timeout.tv_sec = LONG_MAX;
    timeout.tv_usec = 0;
    block = 0;
    pthread_mutex_lock(&_netsnmp_lock);
    snmp_select_info2(&num_fds, &read_fds, &timeout, &block);
    pthread_mutex_unlock(&_netsnmp_lock);

    // Wait for some SNMP work or the timeout to expire, and then process.
    int select_rc = netsnmp_large_fd_set_select(num_fds, &read_fds, NULL, NULL, (!block) ? &timeout : NULL);

    if (select_rc >= 0)
    {
      // Pass the work or the timeout indication to Net-SNMP.
      pthread_mutex_lock(&_netsnmp_lock);
      if (select_rc > 0)
      {
        snmp_read2(&read_fds);
      }
      else if (select_rc == 0)
      {
        snmp_timeout();
      }

      // Process any "alarms" - note these aren't SNMP TRAPs, but timers run by SNMP itself.
      run_alarms();

      // ...and finally process any delegated or queued requests.
      netsnmp_check_outstanding_agent_requests();

      pthread_mutex_unlock(&_netsnmp_lock);
    }
    else if ((select_rc != -1) || (errno != EINTR))
    {
      // Error.  We ignore EINTR, as it can happen spuriously.
      TRC_WARNING("SNMP select failed with RC %d (errno: %d)", select_rc, errno);
    }
  }
};

int Agent::logging_callback(int majorID, int minorID, void* serverarg, void* clientarg)
{
  snmp_log_message* log_message = (snmp_log_message*)serverarg;
  int snmp_priority = log_message->priority;
  int clearwater_priority = Log::STATUS_LEVEL;

  switch (snmp_priority) {
    case LOG_EMERG:
    case LOG_ALERT:
    case LOG_CRIT:
    case LOG_ERR:
      clearwater_priority = Log::ERROR_LEVEL;
      break;
    case LOG_WARNING:
      clearwater_priority = Log::WARNING_LEVEL;
      break;
    case LOG_NOTICE:
      clearwater_priority = Log::STATUS_LEVEL;
      break;
    case LOG_INFO:
      clearwater_priority = Log::INFO_LEVEL;
      break;
    case LOG_DEBUG:
      clearwater_priority = Log::DEBUG_LEVEL;
      break;
  }

  if (clearwater_priority <= Log::loggingLevel)
  {
    char* orig_msg = strdup(log_message->msg);
    char* msg = orig_msg;
    // Remove the trailing newline
    msg[strlen(msg) - 1] = '\0';
    Log::write(clearwater_priority, "(Net-SNMP)", 0, msg);
    free(orig_msg);
  }

  return 0;
}

}

// Set up the SNMP agent. Returns 0 if it succeeds.
int snmp_setup(const char* name)
{
  // First, check if we'd previously instantiated an SNMP agent, and tidy it up
  // if so.
  if (SNMP::Agent::instance() != NULL)
  {
    SNMP::Agent::deinstantiate();
  }

  // Then, instantiate the new one.
  try
  {
    SNMP::Agent::instantiate(name);
    TRC_STATUS("AgentX agent initialised");
    return 0;
  }
  catch (int rc)
  {
    TRC_WARNING("SNMP AgentX initialization failed");
    return rc;
  }
}

// Set up the SNMP handling threads. Returns 0 if it succeeds.
int init_snmp_handler_threads(const char* name)
{
  try
  {
    SNMP::Agent::instance()->start();
    return 0;
  }
  catch (int rc)
  {
    return rc;
  }
}

// Cancel the handler thread and shut down the SNMP agent.
//
// Calling this on CentOS when the thread has not been created in
// init_snmp_handler_threads above is dangerous, and can lead to sig11 death.
// This appears to be a difference in the behaviour of pthread_cancel
// between CentOS and Ubuntu when it is passed NULL.
void snmp_terminate(const char* name)
{
  SNMP::Agent::instance()->stop();

  // We don't deinitialize here to minimize termination race conditions.  We
  // deinitialize the next time snmp_setup is called (if it is called).  Yes,
  // this is a leak, but it's a very small, one-off leak, and it's easier than
  // getting Net-SNMP to terminate cleanly.
}
