/**
 * @file httpresponseparser.cpp
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

#include "httpresponseparser.h"

HttpResponseParser::HttpResponseParser(long* status_code,
                                       std::map<std::string, std::string>* headers,
                                       std::string* body) :
  _status_code(status_code),
  _headers(headers),
  _body(body),
  _seen_header_value(false),
  _complete(false)
{
  http_parser_settings_init(&_parser_settings);

  // If the user is interested in headers, register the appropriate callbacks.
  if (_headers != nullptr)
  {
    _parser_settings.on_header_field = on_header_name_cb;
    _parser_settings.on_header_value = on_header_value_cb;
    _parser_settings.on_headers_complete = on_headers_complete_cb;
  }

  // If the user is interested in the body, register the `on_body` callback.
  if (_body != nullptr)
  {
    _parser_settings.on_body = on_body_cb;
  }

  // Always register the  completion callback, so we can correctly answer calls
  // to `is_complete()`.
  _parser_settings.on_message_complete = on_message_complete_cb;

  http_parser_init(&_parser, HTTP_RESPONSE);
  _parser.data = this;
}

HttpResponseParser::~HttpResponseParser() {}

ssize_t HttpResponseParser::feed(const char* data, size_t len)
{
  ssize_t consumed = http_parser_execute(&_parser, &_parser_settings, data, len);
  return consumed;
}

void HttpResponseParser::on_header_name(const char* data, size_t len)
{
  // We've got some header name data. This could be the start of a new header,
  // so store any previous one.
  commit_header();

  _stored_header_name.append(data, len);
}

void HttpResponseParser::on_header_value(const char* data, size_t len)
{
  _seen_header_value = true;
  _stored_header_value.append(data, len);
}

void HttpResponseParser::on_headers_complete()
{
  // Store the last header in the message.
  commit_header();
}

void HttpResponseParser::on_body(const char* data, size_t len)
{
  _body->append(data, len);
}

void HttpResponseParser::on_message_complete()
{
  _complete = true;

  // If the user has asked for it, store the status code. We always register
  // `on_message_complete` (even if the user does not care about the status
  // code) so we need to check the pointer before writing to it.
  if (_status_code != nullptr)
  {
    *_status_code = _parser.status_code;
  }

  // Pause the parser. This means that if there are multiple responses in the
  // data block we don't go onto to parse the next one.
  http_parser_pause(&_parser, 1);
}

void HttpResponseParser::commit_header()
{
  // We can only commit a header if we've seen the name and the value.
  if (!_stored_header_name.empty() && _seen_header_value)
  {
    _headers->insert(std::make_pair(std::move(_stored_header_name),
                                    std::move(_stored_header_value)));

    // Reset the internal header buffers.
    _seen_header_value = false;
    _stored_header_name.clear();
    _stored_header_value.clear();
  }
}
