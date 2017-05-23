/**
 * @file json_parse_utils.h Utilities for parsing JSON documents.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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

#define JSON_ASSERT_UINT(NODE)                                                 \
{                                                                              \
  if (!(NODE).IsUint())                                                        \
  {                                                                            \
    JSON_FORMAT_ERROR();                                                       \
  }                                                                            \
}

#define JSON_ASSERT_UINT_64(NODE)                                              \
{                                                                              \
  if (!(NODE).IsUint64())                                                      \
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

#define JSON_GET_UINT_MEMBER(NODE, ATTR_NAME, TARGET)                          \
{                                                                              \
    JSON_ASSERT_CONTAINS((NODE), (ATTR_NAME));                                 \
    JSON_ASSERT_UINT((NODE)[(ATTR_NAME)]);                                     \
    (TARGET) = (NODE)[(ATTR_NAME)].GetUint();                                  \
}

#define JSON_GET_UINT_64_MEMBER(NODE, ATTR_NAME, TARGET)                       \
{                                                                              \
    JSON_ASSERT_CONTAINS((NODE), (ATTR_NAME));                                 \
    JSON_ASSERT_UINT_64((NODE)[(ATTR_NAME)]);                                  \
    (TARGET) = (NODE)[(ATTR_NAME)].GetUint64();                                \
}

#define JSON_GET_BOOL_MEMBER(NODE, ATTR_NAME, TARGET)                          \
{                                                                              \
    JSON_ASSERT_CONTAINS((NODE), (ATTR_NAME));                                 \
    JSON_ASSERT_BOOL((NODE)[(ATTR_NAME)]);                                     \
    (TARGET) = (NODE)[(ATTR_NAME)].GetBool();                                  \
}

#define JSON_SAFE_GET_STRING_MEMBER(NODE, ATTR_NAME, TARGET)                   \
{                                                                              \
    if (((NODE).HasMember(ATTR_NAME)) &&                                       \
        ((NODE)[(ATTR_NAME)].IsString()))                                      \
    {                                                                          \
      (TARGET) = (NODE)[(ATTR_NAME)].GetString();                              \
    }                                                                          \
}

#define JSON_SAFE_GET_INT_MEMBER(NODE, ATTR_NAME, TARGET)                      \
{                                                                              \
    if (((NODE).HasMember(ATTR_NAME)) &&                                       \
        ((NODE)[(ATTR_NAME)].IsInt()))                                         \
    {                                                                          \
      (TARGET) = (NODE)[(ATTR_NAME)].GetInt();                                 \
    }                                                                          \
}

#define JSON_SAFE_GET_INT_64_MEMBER(NODE, ATTR_NAME, TARGET)                   \
{                                                                              \
    if (((NODE).HasMember(ATTR_NAME)) &&                                       \
        ((NODE)[(ATTR_NAME)].IsInt64()))                                       \
    {                                                                          \
      (TARGET) = (NODE)[(ATTR_NAME)].GetInt64();                               \
    }                                                                          \
}

#define JSON_SAFE_GET_UINT_MEMBER(NODE, ATTR_NAME, TARGET)                     \
{                                                                              \
    if (((NODE).HasMember(ATTR_NAME)) &&                                       \
        ((NODE)[(ATTR_NAME)].IsUint()))                                        \
    {                                                                          \
      (TARGET) = (NODE)[(ATTR_NAME)].GetUint();                                \
    }                                                                          \
}

#define JSON_SAFE_GET_UINT_64_MEMBER(NODE, ATTR_NAME, TARGET)                  \
{                                                                              \
    if (((NODE).HasMember(ATTR_NAME)) &&                                       \
        ((NODE)[(ATTR_NAME)].IsUint64()))                                      \
    {                                                                          \
      (TARGET) = (NODE)[(ATTR_NAME)].GetUint64();                              \
    }                                                                          \
}

#define JSON_SAFE_GET_BOOL_MEMBER(NODE, ATTR_NAME, TARGET)                     \
{                                                                              \
    if (((NODE).HasMember(ATTR_NAME)) &&                                       \
        ((NODE)[(ATTR_NAME)].IsBool()))                                        \
    {                                                                          \
      (TARGET) = (NODE)[(ATTR_NAME)].GetBool();                                \
    }                                                                          \
}

#endif
