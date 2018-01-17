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

class HttpResponse
{
public:
  HttpResponse(HTTPCode return_code,
               const std::string& resp_body,
               const std::map<std::string, std::string>& resp_headers);

  virtual ~HttpResponse();

  // GET methods return empty if not set yet
  virtual HTTPCode get_return_code(); 
  virtual std::string get_resp_body();
  virtual std::map<std::string, std::string> get_resp_headers();

private:
  HTTPCode _return_code;
  std::string _resp_body;
  std::map<std::string, std::string> _resp_headers;   
};

class HttpRequest
{
public:
  HttpRequest(const std::string& server,
              const std::string& scheme,
              HttpClient* client,
              HttpClient::RequestType method,
              std::string path);

  virtual ~HttpRequest();

  // SET methods will overwrite any previous settings
  virtual void set_req_body(std::string body);
  virtual void set_sas_trail(SAS::TrailId trail);
  virtual void set_allowed_host_state(int allowed_host_state);
  virtual void set_username(std::string username); //Unclear if we ever actually use this atm, may be unnecessary

  // ADD methods
  virtual void add_req_header(std::string req_header);

  // Sends the request and populates ret code, recv headers, and recv body
  virtual HttpResponse send();

private:
  // member variables for storing the request information pre and post send
  std::string _server;
  std::string _scheme;
  HttpClient* _client;
  HttpClient::RequestType _method;
  std::string _path;
  SAS::TrailId _trail;

  std::string _username;
  std::string _req_body;
  std::vector<std::string> _req_headers;
  int _allowed_host_state = BaseResolver::ALL_LISTS;
};
