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
#include "http_request.h"

/// This class manages creation of HttpRequests for a single server and scheme.
/// Allows the user to create HttpRequests without needing to pass the server
/// name and scheme around their code.
///
/// Provides no additional function beyond what is provided by the HttpRequest
/// and HttpClient objects.
class HttpConnection
{
public:
  HttpConnection(const std::string& server,
                 HttpClient* client,
                 const std::string& scheme = "http") :
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
  HttpRequest create_request(HttpClient::RequestType method, std::string path)
  {
    return HttpRequest(_server,
                       _scheme,
                       _client,
                       method,
                       path);
  }

protected:

  std::string _scheme;
  std::string _server;
  HttpClient* _client;
};
