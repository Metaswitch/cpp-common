/**
 * @file http_request.h Definitions for HttpRequest class.
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "httpclient.h"

#ifndef HTTP_RESPONSE_H__
#define HTTP_RESPONSE_H__

class HttpResponse
{
public:
  HttpResponse(HTTPCode return_code,
               const std::string& body,
               const std::map<std::string, std::string>& headers);

  virtual ~HttpResponse();

  virtual HTTPCode get_rc();
  virtual std::string get_body();
  virtual std::map<std::string, std::string> get_headers();

private:
  HTTPCode _rc;
  std::string _body;
  std::map<std::string, std::string> _headers;
};

class HttpRequest
{
public:
  HttpRequest(const std::string server,
              const std::string scheme,
              HttpClient* client,
              HttpClient::RequestType method,
              const std::string path) :
    _server(std::move(server)),
    _scheme(std::move(scheme)),
    _client(client),
    _method(method),
    _path(std::move(path))
  {
  }

  ~HttpRequest();

  // SET methods will overwrite any previous settings
  HttpRequest& set_body(const std::string& body);
  HttpRequest& set_sas_trail(SAS::TrailId trail);
  HttpRequest& set_allowed_host_state(int allowed_host_state);
  HttpRequest& set_username(const std::string& username);

  // ADD methods
  HttpRequest& add_header(const std::string& header);

  // Sends the request and populates ret code, recv headers, and recv body
  HttpResponse send();

private:
  // member variables for storing the request information pre and post send
  std::string _server;
  std::string _scheme;
  HttpClient* _client;
  HttpClient::RequestType _method;
  std::string _path;
  SAS::TrailId _trail = 0;

  std::string _username;
  std::string _body;
  std::vector<std::string> _headers;
  int _allowed_host_state = BaseResolver::ALL_LISTS;
};

#endif
