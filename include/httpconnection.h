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

#include "httpclient.h"

/// Provides managed access to data on a single set of HTTP servers. Properly
/// supports round-robin DNS load balancing.
///
/// This class is a thin wrapper around HttpRequest and HttpClient, and should
/// be used to generate HttpRequests targeted at a single server address
///
class HttpConnection
{
public:
  HttpConnection(const std::string& server,
                 const std::string& scheme = "http",
                 HttpClient* client) :
    _scheme(scheme),
    _server(server),
    _client(client)
  {
    TRC_STATUS("Configuring HTTP Connection");
    TRC_STATUS("  Connection created for server %s", _server.c_str());
  }

  virtual ~HttpConnection()
  {
  }

  /// Create an HttpRequest with our server and scheme arguments
  HttpRequest create_request(path)
  {
    return HttpRequest(_server,
                       _scheme,
                       _client,
                       path);
  }

protected:

  std::string _scheme;
  std::string _server;
  HttpClient _client;
};
