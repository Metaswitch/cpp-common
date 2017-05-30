/**
 * @file http_connection_pool.cpp  Implementation of derived class for HTTP
 * connection pooling.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "http_connection_pool.h"
#include "httpconnection.h"

HttpConnectionPool::HttpConnectionPool(LoadMonitor* load_monitor,
                                       SNMP::IPCountTable* stat_table) :
  ConnectionPool<CURL*>(MAX_IDLE_TIME_S),
  _stat_table(stat_table)
{
  _timeout_ms = calc_req_timeout_from_latency((load_monitor != NULL) ?
                                                              load_monitor->get_target_latency_us() :
                                                              DEFAULT_LATENCY_US);

  TRC_STATUS("Connection pool will use a response timeout of %ldms", _timeout_ms);
}

CURL* HttpConnectionPool::create_connection(AddrInfo target)
{
  CURL* conn = curl_easy_init();
  TRC_DEBUG("Allocated CURL handle %p", conn);

  // Retrieved data will always be written to a string.
  curl_easy_setopt(conn, CURLOPT_WRITEFUNCTION, &HttpClient::string_store);

  // Only keep one TCP connection to a Homestead per CURL, to
  // avoid using unnecessary resources.
  curl_easy_setopt(conn, CURLOPT_MAXCONNECTS, 1L);

  // Maximum time to wait for a response.  This is the target latency for
  // this node plus a delta
  curl_easy_setopt(conn, CURLOPT_TIMEOUT_MS, _timeout_ms);

  // Time to wait until we establish a TCP connection to a single host.
  curl_easy_setopt(conn,
                   CURLOPT_CONNECTTIMEOUT_MS,
                   SINGLE_CONNECT_TIMEOUT_MS);

  // We mustn't reuse DNS responses, because cURL does no shuffling
  // of DNS entries and we rely on this for load balancing.
  curl_easy_setopt(conn, CURLOPT_DNS_CACHE_TIMEOUT, 0L);

  // Nagle is not required. Probably won't bite us, but can't hurt
  // to turn it off.
  curl_easy_setopt(conn, CURLOPT_TCP_NODELAY, 1L);

  // We are a multithreaded app using C-Ares. This is the
  // recommended setting.
  curl_easy_setopt(conn, CURLOPT_NOSIGNAL, 1L);

  // Register a debug callback to record the HTTP transaction.  We also need
  // to set the verbose option (otherwise setting the debug function has no
  // effect).
  curl_easy_setopt(conn,
                   CURLOPT_DEBUGFUNCTION,
                   HttpClient::Recorder::debug_callback);

  curl_easy_setopt(conn, CURLOPT_VERBOSE, 1L);

  increment_statistic(target, conn);

  return conn;
}

void HttpConnectionPool::increment_statistic(AddrInfo target, CURL* conn)
{
  if (_stat_table)
  {
    // Increment the statistic
    char buf[100];
    const char* ip_address = inet_ntop(target.address.af,
                                       &target.address.addr,
                                       buf,
                                       sizeof(buf));
    // The lock is held in the base class when this method is called, so it is
    // safe to access the table
    _stat_table->get(ip_address)->increment();
  }
}

void HttpConnectionPool::decrement_statistic(AddrInfo target, CURL* conn)
{
  if (_stat_table)
  {
    // Decrement the statistic
    char buf[100];
    const char* ip_address = inet_ntop(target.address.af,
                                       &target.address.addr,
                                       buf,
                                       sizeof(buf));
    // The lock is held in the base class when this method is called, so it is
    // safe to access the table
    if (_stat_table->get(ip_address)->decrement() == 0)
    {
      // If the statistic is now zero, remove from the table
      _stat_table->remove(ip_address);
    }
  }
}

void HttpConnectionPool::destroy_connection(AddrInfo target, CURL* conn)
{
  decrement_statistic(target, conn);
  curl_slist *host_resolve = NULL;
  curl_easy_getinfo(conn, CURLINFO_PRIVATE, &host_resolve);
  if (host_resolve != NULL)
  {
    curl_easy_setopt(conn, CURLOPT_PRIVATE, NULL);
    curl_slist_free_all(host_resolve);
  }

  curl_easy_cleanup(conn);
}

void HttpConnectionPool::release_connection(ConnectionInfo<CURL*>* conn_info,
                                            bool return_to_pool)
{
  if (return_to_pool)
  {
    // Reset the CURL handle to the default state, so that settings from one
    // request don't leak into another
    CURL* conn = conn_info->conn;
    curl_easy_setopt(conn, CURLOPT_HTTPHEADER, NULL);
    curl_easy_setopt(conn, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(conn, CURLOPT_WRITEHEADER, NULL);
    curl_easy_setopt(conn, CURLOPT_POSTFIELDS, NULL);
    curl_easy_setopt(conn, CURLOPT_HEADERFUNCTION, NULL);
    curl_easy_setopt(conn, CURLOPT_POST, 0);
  }
  ConnectionPool<CURL*>::release_connection(conn_info, return_to_pool);
}

long HttpConnectionPool::calc_req_timeout_from_latency(int latency_us)
{
  return std::max(1, (latency_us * TIMEOUT_LATENCY_MULTIPLIER) / 1000);
}
