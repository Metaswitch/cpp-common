/**
 * @file chronosconnection.cpp.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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

const std::map<std::string, uint32_t> ChronosConnection::EMPTY_TAGS = std::map<std::string, uint32_t>();

ChronosConnection::ChronosConnection(const std::string& server,
                                     std::string callback_host,
                                     HttpResolver* resolver,
                                     BaseCommunicationMonitor* comm_monitor) :
  _callback_host(callback_host),
  _http(new HttpConnection(server,
                           false,
                           resolver,
                           SASEvent::HttpLogLevel::DETAIL,
                           comm_monitor))
{
}


ChronosConnection::~ChronosConnection()
{
  delete _http;
  _http = NULL;
}

HTTPCode ChronosConnection::send_delete(const std::string& delete_identity,
                                        SAS::TrailId trail)
{
  // The delete identity can be an empty string when a previous put/post has failed.
  if (delete_identity == "")
  {
    // Don't bother sending the timer request to Chronos, as it will just reject it
    // with a 405
    TRC_ERROR("Can't delete a timer with an empty timer id");
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
                                     SAS::TrailId trail,
                                     const std::map<std::string, uint32_t>& tags)
{
  std::string path = "/timers/" +
                     Utils::url_escape(put_identity);
  std::string body = create_body(timer_interval, repeat_for, callback_uri, opaque_data, tags);
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
      return HTTP_BAD_REQUEST;
    }
  }

  return rc;
}

HTTPCode ChronosConnection::send_post(std::string& post_identity,
                                      uint32_t timer_interval,
                                      uint32_t repeat_for,
                                      const std::string& callback_uri,
                                      const std::string& opaque_data,
                                      SAS::TrailId trail,
                                      const std::map<std::string, uint32_t>& tags)
{
  std::string path = "/timers";
  std::string body = create_body(timer_interval, repeat_for, callback_uri, opaque_data, tags);
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
      return HTTP_BAD_REQUEST;
    }
  }

  return rc;
}

HTTPCode ChronosConnection::send_put(std::string& put_identity,
                                     uint32_t timer_interval,
                                     const std::string& callback_uri,
                                     const std::string& opaque_data,
                                     SAS::TrailId trail,
                                     const std::map<std::string, uint32_t>& tags)
{
  return send_put(put_identity, timer_interval, timer_interval, callback_uri, opaque_data, trail, tags);
}

HTTPCode ChronosConnection::send_post(std::string& post_identity,
                                      uint32_t timer_interval,
                                      const std::string& callback_uri,
                                      const std::string& opaque_data,
                                      SAS::TrailId trail,
                                      const std::map<std::string, uint32_t>& tags)
{
  return send_post(post_identity, timer_interval, timer_interval, callback_uri, opaque_data, trail, tags);
}

std::string ChronosConnection::create_body(uint32_t interval,
                                           uint32_t repeat_for,
                                           const std::string& path,
                                           const std::string& opaque_data,
                                           const std::map<std::string, uint32_t>& tags)
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

    writer.String("statistics");
    writer.StartObject();
    {
      writer.String("tag-info");
      writer.StartArray();
      {
        for (std::map<std::string, uint32_t>::const_iterator it = tags.begin();
                                                             it != tags.end();
                                                             ++it)
        {
          writer.StartObject();
          {
            writer.String("type");
            writer.String(it->first.c_str());
            writer.String("count");
            writer.Int(it->second);
          }
          writer.EndObject();
        }
      }
      writer.EndArray();
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
