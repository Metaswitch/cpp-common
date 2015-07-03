/**
 * @file snmp_row.h
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2015 Metaswitch Networks Ltd
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
