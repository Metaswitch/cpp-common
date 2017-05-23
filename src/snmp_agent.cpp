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

static pthread_t snmp_thread_var;

void* snmp_thread(void* data)
{
  while (1)
  {
    agent_check_and_process(1);
  }
  return NULL;
};

int logging_callback(int majorID, int minorID, void* serverarg, void* clientarg)
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
  // Make sure we start as a subagent, not a master agent.
  netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 1);

  // Set the persistent directory to somewhere that the process can write to
  char persistent_file[80];
  strcpy(persistent_file, "/tmp/");
  strcat(persistent_file, name);
  netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PERSISTENT_DIR, persistent_file);

  // Uncomment this line to send AgentX requests over TCP, rather than a unix
  // domain socket, in order to snoop them with tcpdump. You'll also need to
  // replace the agentXSocket line in /etc/snmp/snmpd.conf on your node with
  // 'agentXSocket tcp:localhost:705' and restart snmpd.
  // netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_X_SOCKET, "tcp:localhost:705");

  // Use callback-based logging, and integrate it with the Clearwater logger
  snmp_enable_calllog();
  snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_LOGGING, logging_callback, NULL);

  netsnmp_container_init_list();
  int rc = init_agent(name);
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
  init_snmp(name);

  int ret = pthread_create(&snmp_thread_var, NULL, snmp_thread, NULL);
  return ret;
}

// Cancel the handler thread and shut down the SNMP agent.
//
// Calling this on CentOS when the thread has not been created in
// init_snmp_handler_threads above is dangerous, and can lead to sig11 death.
// This appears to be a difference in the behaviour of pthread_cancel
// between CentOS and Ubuntu when it is passed NULL.
void snmp_terminate(const char* name)
{
  pthread_cancel(snmp_thread_var);
  snmp_shutdown(name);
  netsnmp_container_free_list();
}
