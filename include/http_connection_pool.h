/**
 * @file http_connection_pool.h  Declaration of derived class for HTTP connection
 * pooling.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef HTTP_CONNECTION_POOL_H__
#define HTTP_CONNECTION_POOL_H__

#include <curl/curl.h>

#include "load_monitor.h"
#include "snmp_ip_count_table.h"
#include "connection_pool.h"

/// Total time to wait for a response from a single server as a multiple of the
/// configured target latency before giving up.  This is the value that affects
/// the user experience, so should be set to what we consider acceptable.
/// Covers connection attempt, request and response.  Note that we normally
/// make two requests before giving up, so the maximum total latency is twice
/// this.
static const int TIMEOUT_LATENCY_MULTIPLIER = 5;
static const int DEFAULT_LATENCY_US = 100000;

/// Approximate length of time to wait before giving up on a
/// connection attempt to a single address (in milliseconds).  cURL
/// may wait more or less than this depending on the number of
/// addresses to be tested and where this address falls in the
/// sequence. A connection will take longer than this to establish if
/// multiple addresses must be tried. This includes only the time to
/// perform the DNS lookup and establish the connection, not to send
/// the request or receive the response.
///
/// We set this quite short to ensure we quickly move on to another
/// server. A connection should be very fast to establish (a few
/// milliseconds) in the success case.
static const long SINGLE_CONNECT_TIMEOUT_MS = 50;

/// The length of time a connection can remain idle before it is removed from
/// the pool
static const int MAX_IDLE_TIME_S = 60;

class HttpConnectionPool : public ConnectionPool<CURL*>
{
public:
  HttpConnectionPool(LoadMonitor* load_monitor,
                     SNMP::IPCountTable* stat_table);

  ~HttpConnectionPool()
  {
    // This call is important to properly destroy the connection pool
    destroy_connection_pool();
  }

protected:
  CURL* create_connection(AddrInfo target) override;

  // Handles incrementing the statistic that keeps track of the number of
  // connections to a target
  void increment_statistic(AddrInfo target, CURL* conn);

  // Handles decrementing the statistic that keeps track of the number of
  // connections to a target
  void decrement_statistic(AddrInfo target, CURL* conn);

  void destroy_connection(AddrInfo target, CURL* conn) override;

  // Reset the CURL handle to the default state, then release it into the pool
  void release_connection(ConnectionInfo<CURL*>* conn_info,
                          bool return_to_pool) override;

  long _timeout_ms;
  SNMP::IPCountTable* _stat_table;

  // Determines an appropriate absolute HTTP request timeout in ms given the
  // target latency for requests that the downstream components will be using
  static long calc_req_timeout_from_latency(int latency_us);
};
#endif
