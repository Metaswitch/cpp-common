/**
 * @file chronosconnection.cpp.
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

#include <string>
#include <map>
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "utils.h"
#include "log.h"
#include "sas.h"
#include "sasevent.h"
#include "httpconnection.h"
#include "chronosconnection.h"

ChronosConnection::ChronosConnection(const std::string& server,
                                     std::string callback_host,
                                     HttpResolver* resolver) :
  _callback_host(callback_host),
  _http(new HttpConnection(server,
                           false,
                           resolver,
                           SASEvent::HttpLogLevel::DETAIL))
{
}


ChronosConnection::~ChronosConnection()
{
  delete _http;
  _http = NULL;
}

/// Set a monitor to track HTTP REST communication state, and set/clear
/// alarms based upon recent activity.
void ChronosConnection::set_comm_monitor(CommunicationMonitor* comm_monitor)
{
  _http->set_comm_monitor(comm_monitor);
}

HTTPCode ChronosConnection::send_delete(const std::string& delete_identity,
                                        SAS::TrailId trail)
{
  // The delete identity can be an empty string when a previous put/post has failed.
  if (delete_identity == "")
  {
    // Don't bother sending the timer request to Chronos, as it will just reject it
    // with a 405
    LOG_ERROR("Can't delete a timer with an empty timer id");
    return HTTP_BADMETHOD;
  }

  std::string path = "/timers/" +
                     Utils::url_escape(delete_identity);
  return _http->send_delete(path, trail);
}

HTTPCode ChronosConnection::send_put(std::string& put_identity,
                                     uint32_t timer_interval,
                                     uint32_t repeat_for,
                                     const std::string& callback_uri,
                                     const std::string& opaque_data,
                                     SAS::TrailId trail)
{
  std::string path = "/timers/" +
                     Utils::url_escape(put_identity);
  std::string body = create_body(timer_interval, repeat_for, callback_uri, opaque_data);
  std::map<std::string, std::string> headers;

  HTTPCode rc = _http->send_put(path, headers, body, trail);

  if (rc == HTTP_OK)
  {
    // Try and get the location header from the response
    std::string timer_url = get_location_header(headers);

    if (timer_url != "")
    {
      put_identity = timer_url;
    }
    else
    {
      return HTTP_BAD_RESULT;
    }
  }

  return rc;
}

HTTPCode ChronosConnection::send_post(std::string& post_identity,
                                      uint32_t timer_interval,
                                      uint32_t repeat_for,
                                      const std::string& callback_uri,
                                      const std::string& opaque_data,
                                      SAS::TrailId trail)
{
  std::string path = "/timers";
  std::string body = create_body(timer_interval, repeat_for, callback_uri, opaque_data);
  std::map<std::string, std::string> headers;

  HTTPCode rc = _http->send_post(path, headers, body, trail);

  if (rc == HTTP_OK)
  {
    // Try and get the location header from the response
    std::string timer_url = get_location_header(headers);

    if (timer_url != "")
    {
      post_identity = timer_url;
    }
    else
    {
      return HTTP_BAD_RESULT;
    }
  }

  return rc;
}

HTTPCode ChronosConnection::send_put(std::string& put_identity,
                                     uint32_t timer_interval,
                                     const std::string& callback_uri,
                                     const std::string& opaque_data,
                                     SAS::TrailId trail)
{
  return send_put(put_identity, timer_interval, timer_interval, callback_uri, opaque_data, trail);
}

HTTPCode ChronosConnection::send_post(std::string& post_identity,
                                      uint32_t timer_interval,
                                      const std::string& callback_uri,
                                      const std::string& opaque_data,
                                      SAS::TrailId trail)
{
  return send_post(post_identity, timer_interval, timer_interval, callback_uri, opaque_data, trail);
}

std::string ChronosConnection::create_body(uint32_t interval,
                                           uint32_t repeat_for,
                                           const std::string& path,
                                           const std::string& opaque_data)
{
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

  writer.StartObject();
  {
    writer.String("timing");
    writer.StartObject();
    {
      writer.String("interval");
      writer.Int(interval);
      writer.String("repeat-for");
      writer.Int(repeat_for);
    }
    writer.EndObject();

    writer.String("callback");
    writer.StartObject();
    {
      writer.String("http");
      writer.StartObject();
      {
        writer.String("uri");
        writer.String(std::string("http://" + _callback_host + path).c_str());
        writer.String("opaque");
        writer.String(opaque_data.c_str());
      }
      writer.EndObject();
    }
    writer.EndObject();
  }
  writer.EndObject();

  return sb.GetString();
}

std::string ChronosConnection::get_location_header(std::map<std::string, std::string> headers)
{
  std::string timer_url = headers["location"];

  // Location header has the form "/timers/abcd" -
  // we just want the "abcd" part after "/timers/"
  if (timer_url != "")
  {
    size_t start_of_path = timer_url.find("/timers/") + (std::string("/timers/").length());
    timer_url = timer_url.substr(start_of_path, std::string::npos);
  }

  return timer_url;
}
