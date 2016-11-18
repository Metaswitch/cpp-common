/**
 * @file httpconnection.h Definitions for HttpConnection class.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
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
                 const std::string& scheme = "http") :
    _scheme(scheme),
    _server(server),
    _client(assert_user,
            resolver,
            stat_table,
            load_monitor,
            sas_log_level,
            comm_monitor)
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

  virtual ~HttpConnection() {}

  /// Sends a HTTP GET request to _server with the specified parameters
  ///
  /// @param path           Absolute path to request from server - must start
  ///                       with "/"
  /// @param headers        Location to store the header part of the retrieved
  ///                       data
  /// @param response       Location to store retrieved data
  /// @param username       Username to assert if assertUser is true, else
  ///                       ignored
  /// @param headers_to_add Extra headers to add to the request
  /// @param trail          SAS trail to use
  ///
  /// @returns              HTTP code representing outcome of request
  virtual long send_get(const std::string& path,
                        std::map<std::string, std::string>& headers,
                        std::string& response,
                        const std::string& username,
                        std::vector<std::string> headers_to_add,
                        SAS::TrailId trail);
  virtual long send_get(const std::string& path,
                        std::string& response,
                        const std::string& username,
                        SAS::TrailId trail);
  virtual long send_get(const std::string& path,
                        std::map<std::string, std::string>& headers,
                        std::string& response,
                        const std::string& username,
                        SAS::TrailId trail);

  /// Sends a HTTP DELETE request to _server with the specified parameters
  ///
  /// @param path     Absolute path to request from server - must start
  ///                 with "/"
  /// @param headers  Location to store the header part of the retrieved
  ///                 data
  /// @param response Location to store retrieved data
  /// @param trail    SAS trail to use
  /// @param body     Body to send on the request
  /// @param username Username to assert if assertUser is true, else
  ///                 ignored
  ///
  /// @returns        HTTP code representing outcome of request
  virtual long send_delete(const std::string& path,
                           std::map<std::string, std::string>& headers,
                           std::string& response,
                           SAS::TrailId trail,
                           const std::string& body = "",
                           const std::string& username = "");
  virtual long send_delete(const std::string& path,
                           SAS::TrailId trail,
                           const std::string& body = "");
  virtual long send_delete(const std::string& path,
                           SAS::TrailId trail,
                           const std::string& body,
                           std::string& response);

  /// Sends a HTTP PUT request to _server with the specified parameters
  ///
  /// @param path              Absolute path to request from server - must start
  ///                          with "/"
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
  virtual long send_put(const std::string& path,
                        std::map<std::string, std::string>& headers,
                        std::string& response,
                        const std::string& body,
                        const std::vector<std::string>& extra_req_headers,
                        SAS::TrailId trail,
                        const std::string& username = "");

  virtual long send_put(const std::string& path,
                        const std::string& body,
                        SAS::TrailId trail,
                        const std::string& username = "");
  virtual long send_put(const std::string& path,
                        std::string& response,
                        const std::string& body,
                        SAS::TrailId trail,
                        const std::string& username = "");
  virtual long send_put(const std::string& path,
                        std::map<std::string, std::string>& headers,
                        const std::string& body,
                        SAS::TrailId trail,
                        const std::string& username = "");

  /// Sends a HTTP POST request to _server with the specified parameters
  ///
  /// @param path     Absolute path to request from server - must start
  ///                 with "/"
  /// @param headers  Location to store the header part of the retrieved
  ///                 data
  /// @param response Location to store retrieved data
  /// @param body     Body to send on the request
  /// @param trail    SAS trail to use
  /// @param username Username to assert if assertUser is true, else
  ///                 ignored
  ///
  /// @returns        HTTP code representing outcome of request
  virtual long send_post(const std::string& path,
                         std::map<std::string, std::string>& headers,
                         std::string& response,
                         const std::string& body,
                         SAS::TrailId trail,
                         const std::string& username = "");

  virtual long send_post(const std::string& path,
                         std::map<std::string, std::string>& headers,
                         const std::string& body,
                         SAS::TrailId trail,
                         const std::string& username = "");

private:
  std::string _scheme;
  std::string _server;
  HttpClient _client;
};

