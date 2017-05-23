/**
 * @file httpconnection.cpp HttpConnection class methods.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "httpconnection.h"

HTTPCode HttpConnection::send_delete(const std::string& url_tail,
                                     SAS::TrailId trail,
                                     const std::string& body)
{
  return _client.send_delete(_scheme + "://" + _server + url_tail,
                             trail,
                             body);
}

HTTPCode HttpConnection::send_delete(const std::string& url_tail,
                                     SAS::TrailId trail,
                                     const std::string& body,
                                     std::string& response)
{
  return _client.send_delete(_scheme + "://" + _server + url_tail,
                             trail,
                             body,
                             response);
}

HTTPCode HttpConnection::send_delete(const std::string& url_tail,
                                     std::map<std::string, std::string>& headers,
                                     std::string& response,
                                     SAS::TrailId trail,
                                     const std::string& body,
                                     const std::string& username)
{
  return _client.send_delete(_scheme + "://" + _server + url_tail,
                             headers,
                             response,
                             trail,
                             body,
                             username);
}

HTTPCode HttpConnection::send_put(const std::string& url_tail,
                                  const std::string& body,
                                  SAS::TrailId trail,
                                  const std::string& username)
{
  return _client.send_put(_scheme + "://" + _server + url_tail,
                          body,
                          trail,
                          username);
}

HTTPCode HttpConnection::send_put(const std::string& url_tail,
                                  std::string& response,
                                  const std::string& body,
                                  SAS::TrailId trail,
                                  const std::string& username)
{
  return _client.send_put(_scheme + "://" + _server + url_tail,
                          response,
                          body,
                          trail,
                          username);
}

HTTPCode HttpConnection::send_put(const std::string& url_tail,
                                  std::map<std::string, std::string>& headers,
                                  const std::string& body,
                                  SAS::TrailId trail,
                                  const std::string& username)
{
  return _client.send_put(_scheme + "://" + _server + url_tail,
                          headers,
                          body,
                          trail,
                          username);
}

HTTPCode HttpConnection::send_put(const std::string& url_tail,
                                  std::map<std::string, std::string>& headers,
                                  std::string& response,
                                  const std::string& body,
                                  const std::vector<std::string>& extra_req_headers,
                                  SAS::TrailId trail,
                                  const std::string& username)
{
  return _client.send_put(_scheme + "://" + _server + url_tail,
                          headers,
                          response,
                          body,
                          extra_req_headers,
                          trail,
                          username);
}

HTTPCode HttpConnection::send_post(const std::string& url_tail,
                                   std::map<std::string, std::string>& headers,
                                   const std::string& body,
                                   SAS::TrailId trail,
                                   const std::string& username)
{
  return _client.send_post(_scheme + "://" + _server + url_tail,
                           headers,
                           body,
                           trail,
                           username);
}

HTTPCode HttpConnection::send_post(const std::string& url_tail,
                                   std::map<std::string, std::string>& headers,
                                   std::string& response,
                                   const std::string& body,
                                   SAS::TrailId trail,
                                   const std::string& username)
{
  return _client.send_post(_scheme + "://" + _server + url_tail,
                           headers,
                           response,
                           body,
                           trail,
                           username);
}

/// Get data; return a HTTP return code
HTTPCode HttpConnection::send_get(const std::string& url_tail,
                                  std::string& response,
                                  const std::string& username,
                                  SAS::TrailId trail)
{
  return _client.send_get(_scheme + "://" + _server + url_tail,
                          response,
                          username,
                          trail);
}

/// Get data; return a HTTP return code
HTTPCode HttpConnection::send_get(const std::string& url_tail,
                                  std::map<std::string, std::string>& headers,
                                  std::string& response,
                                  const std::string& username,
                                  SAS::TrailId trail)
{
  return _client.send_get(_scheme + "://" + _server + url_tail,
                          headers,
                          response,
                          username,
                          trail);
}

/// Get data; return a HTTP return code
HTTPCode HttpConnection::send_get(const std::string& url_tail,
                                  std::map<std::string, std::string>& headers,
                                  std::string& response,
                                  const std::string& username,
                                  std::vector<std::string> headers_to_add,
                                  SAS::TrailId trail)
{
  return _client.send_get(_scheme + "://" + _server + url_tail,
                          headers,
                          response,
                          username,
                          headers_to_add,
                          trail);
}
