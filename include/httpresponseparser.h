/**
 * @file httpresponseparser.h
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

#ifndef HTTPRESPONSEPARSER_H__
#define HTTPRESPONSEPARSER_H__

extern "C" {
#include "http_parser.h"
};

#include <string>
#include <map>

class HttpResponseParser
{
public:
  /// Constructor.
  ///
  /// This class is constructed with pointer to locations to store the status
  /// code, header and body. Any of these can be null meaning the element will
  /// not be stored. This means that if the caller does not care about an
  /// element it is ignored efficiently (rather than being stored and
  /// discarded).
  ///
  /// @param status_code - Pointer to where to store the parsed status code.
  /// @param headers     - Pointer to a map to store the parsed headers.
  /// @param body        - Pointer to a string to store the parsed body.
  HttpResponseParser(long* status_code,
                     std::map<std::string, std::string>* headers,
                     std::string* body);

  /// Destructor.
  virtual ~HttpResponseParser();

  /// Feed some data into the parser.
  ///
  /// @param data - Pointer to the data to parse.  @param len  - Length of the
  /// data.
  ///
  /// @return     - Any non negative value indicates the amount of data consumed
  /// by the parser. A negative value indicates a parse error.
  ssize_t feed(const char* data, size_t len);

  /// @return whether the response is complete.
  bool is_complete() { return _complete; }

private:
  // Pointers to where to store the status code, headers and body.
  long* _status_code; std::map<std::string, std::string>* _headers; std::string*
    _body;

  // The underlying parser and its configuration.
  http_parser_settings _parser_settings; http_parser _parser;

  // The parser calls callbacks when it encounters part of a header name or
  // value. These are buffered here until we have an entire header at which
  // point it is committed to the location pointed at by `_headers`.
  //
  // Two triggers cause us to commit a header: *  We see the start of a new
  // header, indicated by a `on_header_field` callback.  *  We see get a
  // `on_headers_complete` callback.
  //
  // We have to be careful with the first trigger. Due to message fragmentation
  // we may see the following sequence of callbacks for a single header.
  // 1. on_header_field
  // 2. on_header_field
  // 3. on_header_value
  //
  // We must be careful not to commit the header on callback 2. However we
  // can't make this decision simply based on whether the
  // `_stored_header_value` is non-empty, as headers are allowed to have no
  // value. Therefore we flag whether we've seen any header value (even if it
  // was empty) and only commit if the flag is set.
  std::string _stored_header_name;
  std::string _stored_header_value;
  bool _seen_header_value;

  // Whether the message is complete.
  bool _complete;

  /// If possible, commit the current header to the user's map.
  ///
  /// It might not always be possible to commit a header, e.g. if the value has
  /// not yet been obtained.
  void commit_header();

  // Boilerplate for defining callbacks to register with the parser. Each
  // callback involves two methods:
  // * A member method called <callback_name>.
  // * A static method called <callback name>_cb. This wraps the member method
  //   and is the thing that's actually registered with the parser.
#define DEFINE_DATA_CB(NAME)                                                   \
  static inline int NAME##_cb(http_parser* parser, const char* data, size_t len)\
  {                                                                            \
    ((HttpResponseParser*)parser->data)->NAME(data, len);                      \
    return 0;                                                                  \
  }                                                                            \
  void NAME(const char* data, size_t len);

#define DEFINE_NO_DATA_CB(NAME)                                                \
  static inline int NAME##_cb(http_parser* parser)                             \
  {                                                                            \
    ((HttpResponseParser*)parser->data)->NAME();                               \
    return 0;                                                                  \
  }                                                                            \
  void NAME();

  DEFINE_DATA_CB(on_header_name);
  DEFINE_DATA_CB(on_header_value);
  DEFINE_DATA_CB(on_body);
  DEFINE_NO_DATA_CB(on_headers_complete);
  DEFINE_NO_DATA_CB(on_message_complete);
};

#endif

