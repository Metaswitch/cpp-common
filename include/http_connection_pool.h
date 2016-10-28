/**
 * @file http_connection_pool.h  Declaration of derived class for HTTP connection
 * pooling.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2016  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
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
