/**
 * @file snmp_table.h
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

#include "snmp_includes.h"
#include "log.h"

#ifndef SNMP_TABLE_H
#define SNMP_TABLE_H

int netsnmp_table_handler_fn(netsnmp_mib_handler *handler,
                             netsnmp_handler_registration *reginfo,
                             netsnmp_agent_request_info *reqinfo,
                             netsnmp_request_info *requests);

namespace SNMP
{

// Wraps a typed SNMP value (raw bytes with a type and size) for ease-of-use.
class Value
{
public:
  // Utility constructor for ASN_UNSIGNEDs
  static Value uint(uint32_t val)
  {
    return Value(ASN_UNSIGNED, (u_char*)&val, sizeof(uint32_t));
  };

  // Utility constructor for ASN_INTEGERS
  static Value integer(int val)
  {
    return Value(ASN_INTEGER, (u_char*)&val, sizeof(int32_t));
  };


  Value(): type(0), size(0), value(NULL) {};

  // Constructor - copy the raw bytes to avoid lifetime issues
  Value(int type_p, const u_char* value_p, int size_p) :
    type(type_p),
    size(size_p),
    value(new u_char[size])
  {
    memcpy(value, value_p, size);
  };

  ~Value()
  {
    delete[](value); value = NULL;
  }

  int type;
  int size;
  u_char* value;
  
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
   value(new u_char[size])
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
      value = new u_char[size];
      memcpy(value, other.value, size);
    }
    return *this;
  };



};

// A ColumnData is the information for a particular row, implemented as a map of column number to
// its value.
typedef std::map<int, Value> ColumnData;

// Abstract SNMPRowGroup class.
class Row
{
public:
  Row()
  {
    _row = netsnmp_tdata_create_row();
    _row->data = this;
  }
  virtual ~Row()
  {
    netsnmp_tdata_delete_row(_row);
  }
  virtual ColumnData get_columns() = 0;

  netsnmp_tdata_row* get_netsnmp_row() { return _row; };
  netsnmp_tdata_row* _row;

};

// Generic SNMPTable class wrapping a netsnmp_tdata.
template<class T> class Table
{
public:
  Table(std::string name,
            oid* tbl_oid,
            int oidlen):
    _name(name),
    _tbl_oid(tbl_oid),
    _oidlen(oidlen),
    _handler_reg(NULL)
  {
    _table = netsnmp_tdata_create_table(_name.c_str(), 0);
    _table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
  }

  virtual ~Table()
  {
    netsnmp_unregister_handler(_handler_reg);
    snmp_free_varbind(_table->indexes_template);
    snmp_free_varbind(_table_info->indexes);
    netsnmp_tdata_delete_table(_table);
    SNMP_FREE(_table_info);
  }

  void add_index(int type)
  {
    netsnmp_tdata_add_index(_table, ASN_INTEGER);
    netsnmp_table_helper_add_index(_table_info, ASN_INTEGER);
  }

  void set_visible_columns(int min, int max) 
  {
    _table_info->min_column = min;
    _table_info->max_column = max;
  }

  // Registers an SNMP handler for this table. Subclasses should call this in their constructor
  // after setting appropriate indexes.
  void register_tbl()
  {
    TRC_INFO("Registering SNMP table %s", _name.c_str());
    _handler_reg = netsnmp_create_handler_registration(_name.c_str(),
                                                       netsnmp_table_handler_fn,
                                                       _tbl_oid,
                                                       _oidlen,
                                                       HANDLER_CAN_RONLY | HANDLER_CAN_GETBULK);

    netsnmp_tdata_register(_handler_reg,
                           _table,
                           _table_info);
  }


  // Add the rows represented by a SNMPRowGroup into the underlying table.
  void add(T* row)
  {
    if (_handler_reg == NULL)
    {
      register_tbl();
    }
    netsnmp_tdata_add_row(_table, row->get_netsnmp_row());
  };

  // Remove the rows represented by a SNMPRowGroup from the underlying table.
  void remove(T* row)
  {
    netsnmp_tdata_remove_row(_table, row->get_netsnmp_row());
  };


  std::string _name;
  oid* _tbl_oid;
  int _oidlen;
  netsnmp_handler_registration* _handler_reg;
  netsnmp_table_registration_info* _table_info;
  netsnmp_tdata* _table;
};

// Generic ManagedSNMPTable, wrapping an SNMPTable and managing the ownership of rows.
template<class TRow, class TRowKey> class ManagedTable
{
public:
  ManagedTable(std::string name,
                   oid* tbl_oid,
                   int oidlen) :
  _tbl(name, tbl_oid, oidlen)
  {
  }

  virtual ~ManagedTable()
  {
    for (typename std::map<TRowKey, TRow*>::iterator ii = _map.begin();
         ii != _map.end();
         ii++)
    {
      TRow* row = ii->second;
      _tbl.remove(row);
      delete row;
    }
  }

  void add_index(int type) { _tbl.add_index(type); };
  void set_visible_columns(int min, int max) { _tbl.set_visible_columns(min, max); }

  virtual TRow* new_row(TRowKey key) = 0;

  // Returns the row keyed off `key`, creating it if it does not already exist.
  void add_row(TRowKey key)
  {
    TRow* row = new_row(key);
    std::pair<TRowKey, TRow*> new_entry(key, row);
    _map.insert(new_entry);

    _tbl.add(row);
  }

  // Returns the row keyed off `key`, creating it if it does not already exist.
  TRow* get(TRowKey key)
  {
    if (_map.find(key) == _map.end())
    {
      add_row(key);
    }

    return _map.at(key);
  }

  // Deletes the row keyed off `key`.
  void remove(TRowKey key)
  {
    TRow* row = _map.at(key);
    _map.erase(key);
    _tbl.remove(row);
    delete row;
  };

  Table<TRow> _tbl;
  std::map<TRowKey, TRow*> _map;
};

// Generic ManagedSNMPTable, wrapping an SNMPTable and managing the ownership of rows.
template<class TRow, class TRowKey> class SimpleManagedTable : public ManagedTable<TRow, TRowKey>
{
public:
  SimpleManagedTable(std::string name,
                   oid* tbl_oid,
                   int oidlen) :
  ManagedTable<TRow, TRowKey>(name, tbl_oid, oidlen) {};

  TRow* new_row(TRowKey key)
  {
    return new TRow(key);
  }

};


}
#endif
