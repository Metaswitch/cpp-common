/**
 * @file httpconnection.h Definitions for HttpConnection class.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#pragma once

#include <string>

#include "httpresolver.h"
#include "snmp_ip_count_table.h"
#include "load_monitor.h"
#include "sasevent.h"
#include "communicationmonitor.h"
#include "httpclient.h"

/// Provides managed access to data on a single HTTP server. Properly
/// supports round-robin DNS load balancing.
///
/// This class is a thin wrapper around HttpClient.
///
class HttpConnection
{
public:
  HttpConnection(const std::string& server,
                 bool assert_user,
                 HttpResolver* resolver,
                 SNMP::IPCountTable* stat_table,
                 LoadMonitor* load_monitor,
                 SASEvent::HttpLogLevel sas_log_level,
                 BaseCommunicationMonitor* comm_monitor,
                 const std::string& scheme = "http",
                 bool should_omit_body = false) :
    _scheme(scheme),
    _server(server),
    _client(assert_user,
            resolver,
            stat_table,
            load_monitor,
            sas_log_level,
            comm_monitor,
            should_omit_body)
  {
    TRC_STATUS("Configuring HTTP Connection");
    TRC_STATUS("  Connection created for server %s", _server.c_str());
  }

  HttpConnection(const std::string& server,
                 bool assert_user,
                 HttpResolver* resolver,
                 SASEvent::HttpLogLevel sas_log_level,
                 BaseCommunicationMonitor* comm_monitor) :
    HttpConnection(server,
                   assert_user,
                   resolver,
                   NULL,
                   NULL,
                   sas_log_level,
                   comm_monitor)
  {}

  virtual ~HttpConnection()
  {
  }

  /// Sends a HTTP GET request to _server with the specified parameters
  ///
  /// @param url_tail       Everything after the server part of the URL - must
  ///                       start with "/" and can contain path, query and
  ///                       fragment parts.
  /// @param headers        Location to store the header part of the retrieved
  ///                       data
  /// @param response       Location to store retrieved data
  /// @param username       Username to assert if assertUser is true, else
  ///                       ignored
  /// @param headers_to_add Extra headers to add to the request
  /// @param trail          SAS trail to use
  ///
  /// @returns              HTTP code representing outcome of request
  virtual long send_get(const std::string& url_tail,
                        std::map<std::string, std::string>& headers,
                        std::string& response,
                        const std::string& username,
                        std::vector<std::string> headers_to_add,
                        SAS::TrailId trail);
  virtual long send_get(const std::string& url_tail,
                        std::string& response,
                        const std::string& username,
                        SAS::TrailId trail);
  virtual long send_get(const std::string& url_tail,
                        std::map<std::string, std::string>& headers,
                        std::string& response,
                        const std::string& username,
                        SAS::TrailId trail);

  /// Sends a HTTP DELETE request to _server with the specified parameters
  ///
  /// @param url_tail Everything after the server part of the URL - must
  ///                 start with "/". Contains path, query and fragment
  ///                 parts.
  /// @param headers  Location to store the header part of the retrieved
  ///                 data
  /// @param response Location to store retrieved data
  /// @param trail    SAS trail to use
  /// @param body     Body to send on the request
  /// @param username Username to assert if assertUser is true, else
  ///                 ignored
  ///
  /// @returns        HTTP code representing outcome of request
  virtual long send_delete(const std::string& url_tail,
                           std::map<std::string, std::string>& headers,
                           std::string& response,
                           SAS::TrailId trail,
                           const std::string& body = "",
                           const std::string& username = "");
  virtual long send_delete(const std::string& url_tail,
                           SAS::TrailId trail,
                           const std::string& body = "");
  virtual long send_delete(const std::string& url_tail,
                           SAS::TrailId trail,
                           const std::string& body,
                           std::string& response);

  /// Sends a HTTP PUT request to _server with the specified parameters
  ///
  /// @param url_tail          Everything after the server part of the URL - must
  ///                          start with "/" and can contain path, query and
  ///                          fragment parts
  /// @param headers           Location to store the header part of the retrieved
  ///                          data
  /// @param response          Location to store retrieved data
  /// @param body              Body to send on the request
  /// @param extra_req_headers Extra headers to add to the request
  /// @param trail             SAS trail to use
  /// @param username          Username to assert if assertUser is true, else
  ///                          ignored
  ///
  /// @returns                 HTTP code representing outcome of request
  virtual long send_put(const std::string& url_tail,
                        std::map<std::string, std::string>& headers,
                        std::string& response,
                        const std::string& body,
                        const std::vector<std::string>& extra_req_headers,
                        SAS::TrailId trail,
                        const std::string& username = "");

  virtual long send_put(const std::string& url_tail,
                        const std::string& body,
                        SAS::TrailId trail,
                        const std::string& username = "");
  virtual long send_put(const std::string& url_tail,
                        std::string& response,
                        const std::string& body,
                        SAS::TrailId trail,
                        const std::string& username = "");
  virtual long send_put(const std::string& url_tail,
                        std::map<std::string, std::string>& headers,
                        const std::string& body,
                        SAS::TrailId trail,
                        const std::string& username = "");

  /// Sends a HTTP POST request to _server with the specified parameters
  ///
  /// @param url_tail Everything after the server part of the URL - must
  ///                 start with "/" and can contain path, query and
  ///                 fragment parts
  /// @param headers  Location to store the header part of the retrieved
  ///                 data
  /// @param response Location to store retrieved data
  /// @param body     Body to send on the request
  /// @param trail    SAS trail to use
  /// @param username Username to assert if assertUser is true, else
  ///                 ignored
  ///
  /// @returns        HTTP code representing outcome of request
  virtual long send_post(const std::string& url_tail,
                         std::map<std::string, std::string>& headers,
                         std::string& response,
                         const std::string& body,
                         SAS::TrailId trail,
                         const std::string& username = "");

  virtual long send_post(const std::string& url_tail,
                         std::map<std::string, std::string>& headers,
                         const std::string& body,
                         SAS::TrailId trail,
                         const std::string& username = "");

protected:

  std::string _scheme;
  std::string _server;
  HttpClient _client;
};
