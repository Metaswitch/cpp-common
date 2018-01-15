/**
 * @file http_request.cpp HttpRequest class methods.
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "http_request.h"

/// Create an HTTP Request builder object.
///
/// @param server Server to send requests to
/// @param assert_user Assert user in header?
/// @param resolver HTTP resolver to use to resolve server's IP addresses
/// @param stat_name SNMP table to report connection info to.
/// @param load_monitor Load Monitor.
/// @param sas_log_level the level to log HTTP flows at (none/protocol/detail).
/// @param log_display_address Log an address other than server to SAS?
/// @param server_display_address The address to log in the SAS call flow
HttpRequestBuilder::HttpRequestBuilder(const std::string server,
                                       bool assert_user,
                                       HttpResolver* resolver,
                                       SNMP::IPCountTable* stat_table,
                                       LoadMonitor* load_monitor,
                                       SASEvent::HttpLogLevel sas_log_level,
                                       BaseCommunicationMonitor* comm_monitor,
                                       const std::string& scheme = "http",
                                       bool should_omit_body = false,
                                       bool remote_connection = false,
                                       long timeout_ms = -1,
                                       bool log_display_address = false,
                                       std::string server_display_address = "") :
  _scheme(scheme),
  _server(server),
  _assert_user(assert_user),
  _resolver(resolver),
  _load_monitor(load_monitor),
  _sas_log_level(sas_log_level),
  _comm_monitor(comm_monitor),
  _stat_table(stat_table),
  _conn_pool(load_monitor, stat_table, remote_connection, timeout_ms),
  _should_omit_body(should_omit_body),
  _log_display_address(log_display_address),
  _server_display_address(server_display_address),
  _default_allowed_host_state(BaseResolver::ALL_LISTS)
{
  pthread_key_create(&_uuid_thread_local, cleanup_uuid);
  pthread_mutex_init(&_lock, NULL);
  curl_global_init(CURL_GLOBAL_DEFAULT);

  TRC_STATUS("Configuring HTTP Connection");
  TRC_STATUS("  Connection created for server %s", _server.c_str());
}

//TODO other constructor variations

HttpRequestBuilder::~HttpRequestBuilder()
{
//TODO what does this tie into
  RandomUUIDGenerator* uuid_gen =
    (RandomUUIDGenerator*)pthread_getspecific(_uuid_thread_local);

  if (uuid_gen != NULL)
  {
    pthread_setspecific(_uuid_thread_local, NULL);
    cleanup_uuid(uuid_gen); uuid_gen = NULL;
  }

  pthread_key_delete(_uuid_thread_local);
}


// HTTP Request object
HttpRequestBuilder::HttpRequest::HttpRequest(int default_allowed_host_state)
{
  _allowed_host_state(default_allowed_host_state)
}

HttpRequestBuilder::HttpRequest::~HttpRequest() {}


