/**
 * @file chronosconnection.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef CHRONOSCONNECTION_H__
#define CHRONOSCONNECTION_H__

#include <curl/curl.h>
#include "sas.h"

#include "httpconnection.h"
#include "load_monitor.h"

/// @class ChronosConnection
class ChronosConnection
{
public:
  ChronosConnection(const std::string& chronos,
                    std::string callback_host,
                    HttpResolver* resolver,
                    BaseCommunicationMonitor* comm_monitor);
  virtual ~ChronosConnection();

  virtual HTTPCode send_delete(const std::string& delete_id,
                               SAS::TrailId trail);
  virtual HTTPCode send_put(std::string& put_identity,
                            uint32_t timer_interval,
                            uint32_t repeat_for,
                            const std::string& callback_uri,
                            const std::string& opaque_data,
                            SAS::TrailId trail,
                            const std::map<std::string, uint32_t>& tags = EMPTY_TAGS);
  virtual HTTPCode send_post(std::string& post_identity,
                             uint32_t timer_interval,
                             uint32_t repeat_for,
                             const std::string& callback_uri,
                             const std::string& opaque_data,
                             SAS::TrailId trail,
                             const std::map<std::string, uint32_t>& tags = EMPTY_TAGS);

  // Versions without repeat_for (i.e. timers that only fire once)
  virtual HTTPCode send_put(std::string& put_identity,
                            uint32_t timer_interval,
                            const std::string& callback_uri,
                            const std::string& opaque_data,
                            SAS::TrailId trail,
                            const std::map<std::string, uint32_t>& tags = EMPTY_TAGS);
  virtual HTTPCode send_post(std::string& post_identity,
                             uint32_t timer_interval,
                             const std::string& callback_uri,
                             const std::string& opaque_data,
                             SAS::TrailId trail,
                             const std::map<std::string, uint32_t>& tags = EMPTY_TAGS);
  std::string _callback_host;

private:
  std::string create_body(uint32_t expires,
                          uint32_t repeat_for,
                          const std::string& callback_uri,
                          const std::string& opaque_data,
                          const std::map<std::string, uint32_t>& tags);
  std::string get_location_header(std::map<std::string, std::string> headers);
  HttpConnection* _http;
  static const std::map<std::string, uint32_t> EMPTY_TAGS;
};

#endif
