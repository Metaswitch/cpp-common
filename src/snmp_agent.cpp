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

#include "snmp_internal/snmp_includes.h"
#include "snmp_agent.h"
#include "log.h"

SNMPAgent* SNMPAgent::_instance = NULL;

int SNMPAgent::initialize()
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

  pthread_mutex_unlock(&_netsnmp_lock);

  return rc;
}

int SNMPAgent::start(void)
{
  pthread_mutex_lock(&_netsnmp_lock);
  init_snmp(_name.c_str());
  pthread_mutex_unlock(&_netsnmp_lock);

  int rc = pthread_create(&_thread, NULL, thread_fn, this);
  return rc;
}

void SNMPAgent::stop(void)
{
  pthread_cancel(_thread);

  pthread_mutex_lock(&_netsnmp_lock);
  snmp_shutdown(_name.c_str());
  netsnmp_container_free_list();
  pthread_mutex_unlock(&_netsnmp_lock);
}

void SNMPAgent::add_row_to_table(netsnmp_tdata* table, netsnmp_tdata_row* row)
{
  pthread_mutex_lock(&_netsnmp_lock);
  netsnmp_tdata_add_row(table, row);
  pthread_mutex_unlock(&_netsnmp_lock);
}

void SNMPAgent::remove_row_from_table(netsnmp_tdata* table, netsnmp_tdata_row* row)
{
  pthread_mutex_lock(&_netsnmp_lock);
  netsnmp_tdata_remove_row(table, row);
  pthread_mutex_unlock(&_netsnmp_lock);
}

void* SNMPAgent::thread_fn(void* snmp_agent)
{
  ((SNMPAgent*)snmp_agent)->thread_fn();
  return NULL;
}

void SNMPAgent::thread_fn()
{
  while (1)
  {
    // Set up some variables and call into Net-SNMP to initialize them ready
    // for the select call.
    int num_fds = 0;
    fd_set read_fds;
    FD_ZERO(&read_fds);
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    int block = 0;
    pthread_mutex_lock(&_netsnmp_lock);
    snmp_select_info(&num_fds, &read_fds, &timeout, &block);
    pthread_mutex_unlock(&_netsnmp_lock);

    // Wait for some SNMP work or the timeout to expire, and then process.
    int select_rc = select(num_fds, &read_fds, NULL, NULL, (!block) ? &timeout : NULL);

    if (select_rc >= 0)
    {
      // Pass the work or the timeout indication to Net-SNMP.
      pthread_mutex_lock(&_netsnmp_lock);
      if (select_rc > 0)
      {
        snmp_read(&read_fds);
      }
      else if (select_rc == 0)
      {
        snmp_timeout();
      }
      pthread_mutex_unlock(&_netsnmp_lock);
    }
    else if ((select_rc != -1) || (errno != EINTR))
    {
      // Error.  We ignore EINTR, as it can happen spuriously.
      TRC_WARNING("SNMP select failed with RC %d (errno: %d)", select_rc, errno);
    }
  }
};

int SNMPAgent::logging_callback(int majorID, int minorID, void* serverarg, void* clientarg)
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

// Set up the SNMP agent. Returns 0 if it succeeds.
int snmp_setup(const char* name)
{
  SNMPAgent::instance(new SNMPAgent(name));
  int rc = SNMPAgent::instance()->initialize();
  if (rc != 0)
  {
    TRC_WARNING("SNMP AgentX initialization failed");
  }
  else
  {
    TRC_STATUS("AgentX agent initialised");
  }
  return rc;
}

// Set up the SNMP handling threads. Returns 0 if it succeeds.
int init_snmp_handler_threads(const char* name)
{
  return SNMPAgent::instance()->start();
}

// Cancel the handler thread and shut down the SNMP agent.
//
// Calling this on CentOS when the thread has not been created in
// init_snmp_handler_threads above is dangerous, and can lead to sig11 death.
// This appears to be a difference in the behaviour of pthread_cancel
// between CentOS and Ubuntu when it is passed NULL.
void snmp_terminate(const char* name)
{
  SNMPAgent::instance()->stop();
}
