/**
 * @file json_parse_utils.h Utilities for parsing JSON documents.
 *
 * Project Clearwater - IMS in the cloud.
 * Copyright (C) 2015  Metaswitch Networks Ltd
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

#ifndef JSON_PARSE_UTILS_H_
#define JSON_PARSE_UTILS_H_

#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"

// Clearwater code that handles JSON typically uses rapidjson to parse JSON text
// into a DOM. However as JSON is schemaless the code then needs to validate the
// JSON before it can safely read it (e.g. to check that a field that should be
// a string is *actually* a string). This validation adds a lot of line noise to
// the code.
//
// This header file provides a series of macros that validate (parts of) a JSON
// value before reading it. If any of these validations fail a `JsonFormatError`
// exception is thrown. The calling code must catch this exception and take
// recovery action.

/// Object that is thrown when a JSON formatting error is spotted.
struct JsonFormatError
{
  /// Constructor.
  ///
  /// @param file - The file of code that generated this error.
  /// @param line - The line of code that generated this error.
  JsonFormatError(const char* file, int line) : _file(file), _line(line) {}

  /// File on which the error was spotted.
  const char* _file;

  /// Line number in the above file on which the error was spotted.
  const int _line;
};

/// Helper macro to build and throw a format error.
#define JSON_FORMAT_ERROR()                                                    \
{                                                                              \
  throw JsonFormatError(__FILE__, __LINE__);                                   \
}

//
// Helper macros to check that a given JSON value is of the specified type.
//

#define JSON_ASSERT_OBJECT(NODE)                                               \
{                                                                              \
  if (!(NODE).IsObject())                                                      \
  {                                                                            \
    JSON_FORMAT_ERROR();                                                       \
  }                                                                            \
}

#define JSON_ASSERT_INT(NODE)                                                  \
{                                                                              \
  if (!(NODE).IsInt())                                                         \
  {                                                                            \
    JSON_FORMAT_ERROR();                                                       \
  }                                                                            \
}

#define JSON_ASSERT_INT_64(NODE)                                               \
{                                                                              \
  if (!(NODE).IsInt64())                                                       \
  {                                                                            \
    JSON_FORMAT_ERROR();                                                       \
  }                                                                            \
}

#define JSON_ASSERT_STRING(NODE)                                               \
{                                                                              \
  if (!(NODE).IsString())                                                      \
  {                                                                            \
    JSON_FORMAT_ERROR();                                                       \
  }                                                                            \
}

#define JSON_ASSERT_ARRAY(NODE)                                                \
{                                                                              \
  if (!(NODE).IsArray())                                                       \
  {                                                                            \
    JSON_FORMAT_ERROR();                                                       \
  }                                                                            \
}

#define JSON_ASSERT_BOOL(NODE)                                                 \
{                                                                              \
  if (!(NODE).IsBool())                                                        \
  {                                                                            \
    JSON_FORMAT_ERROR();                                                       \
  }                                                                            \
}

// Check that a JSON object contains an attribute with the specified name.
#define JSON_ASSERT_CONTAINS(NODE, ATTR_NAME)                                  \
{                                                                              \
  if (!(NODE).HasMember(ATTR_NAME))                                            \
  {                                                                            \
    JSON_FORMAT_ERROR();                                                       \
  }                                                                            \
}

//
// Helper macros to get the value of an attribute from a JSON object.
//

#define JSON_GET_STRING_MEMBER(NODE, ATTR_NAME, TARGET)                        \
{                                                                              \
    JSON_ASSERT_CONTAINS((NODE), (ATTR_NAME));                                 \
    JSON_ASSERT_STRING((NODE)[(ATTR_NAME)]);                                   \
    (TARGET) = (NODE)[(ATTR_NAME)].GetString();                                \
}

#define JSON_GET_INT_MEMBER(NODE, ATTR_NAME, TARGET)                           \
{                                                                              \
    JSON_ASSERT_CONTAINS((NODE), (ATTR_NAME));                                 \
    JSON_ASSERT_INT((NODE)[(ATTR_NAME)]);                                      \
    (TARGET) = (NODE)[(ATTR_NAME)].GetInt();                                   \
}

#define JSON_GET_INT_64_MEMBER(NODE, ATTR_NAME, TARGET)                        \
{                                                                              \
    JSON_ASSERT_CONTAINS((NODE), (ATTR_NAME));                                 \
    JSON_ASSERT_INT_64((NODE)[(ATTR_NAME)]);                                   \
    (TARGET) = (NODE)[(ATTR_NAME)].GetInt64();                                 \
}

#define JSON_GET_BOOL_MEMBER(NODE, ATTR_NAME, TARGET)                          \
{                                                                              \
    JSON_ASSERT_CONTAINS((NODE), (ATTR_NAME));                                 \
    JSON_ASSERT_BOOL((NODE)[(ATTR_NAME)]);                                     \
    (TARGET) = (NODE)[(ATTR_NAME)].GetBool();                                  \
}

#endif
