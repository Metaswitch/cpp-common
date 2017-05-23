/**
 * @file snmp_row.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <vector>
#include <map>
#include <string>
#include <string.h>

#include "log.h"

#ifndef SNMP_ROW_H
#define SNMP_ROW_H

// Forward-declare netsmnmp_tdata_row, so we don't require everyone who includes this header to
// include all of netsnmp.
struct netsnmp_tdata_row_s;
typedef netsnmp_tdata_row_s netsnmp_tdata_row;

namespace SNMP
{

// Wraps a typed SNMP value (raw bytes with a type and size) for ease-of-use.
class Value
{
public:
  // Utility constructor for ASN_UNSIGNEDs
  static Value uint(uint32_t val);
  // Utility constructor for ASN_INTEGERS
  static Value integer(int val);

  // Empty constructor so this can be easily stored in a std::map.
  Value(): type(0), size(0), value(NULL) {};

  // Constructor - copy the raw bytes to avoid lifetime issues
  Value(int type_p, const unsigned char* value_p, int size_p) :
    type(type_p),
    size(size_p),
    value(new unsigned char[size])
  {
    memcpy(value, value_p, size);
  };

  ~Value()
  {
    delete[](value); value = NULL;
  }

  int type;
  int size;
  unsigned char* value;

  // Move constructor
  Value(Value&& old):
    type(old.type),
    size(old.size),
    value(old.value)
  {
    old.value = NULL;
  };

  // Copy constructor
  Value(const Value& old):
    type(old.type),
    size(old.size),
    value(new unsigned char[size])
  {
    memcpy(value, old.value, size);
  };

  // Assignment operator
  Value& operator=(const Value& other)
  {
    if (this != &other)
    {
      delete[](value);

      size = other.size;
      type = other.type;
      value = new unsigned char[size];
      memcpy(value, other.value, size);
    }
    return *this;
  };
};


// A ColumnData is the information for a particular row, implemented as a map of column number to
// its value.
typedef std::map<int, Value> ColumnData;

template<class T> class Table;

// Abstract Row class which wraps a netsnmp_tdata_row.
class Row
{
public:
  template<class T> friend class Table;
  Row();

  virtual ~Row();

  virtual ColumnData get_columns() = 0;

protected:
  netsnmp_tdata_row* _row;
  netsnmp_tdata_row* get_netsnmp_row() { return _row; };
};

} // namespace SNMP
#endif
