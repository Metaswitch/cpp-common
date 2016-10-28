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

#include "snmp_row.h"
#include "snmp_includes.h"
#include "log.h"

#ifndef SNMP_TABLE_H
#define SNMP_TABLE_H

namespace SNMP
{
// A ColumnData is the information for a particular row, implemented as a map of column number to
// its value.
typedef std::map<int, Value> ColumnData;

template<class T> class Table;

// Generic SNMPTable class wrapping a netsnmp_tdata and netsnmp_table_registration_info and exposing
// an API for manipulating them easily. Doesn't need subclassing, but should usually be wrapped in a
// ManagedTable subclass for convenience.
template<class T> class Table
{
public:
  Table(std::string name, // Name of this table, for logging
        std::string tbl_oid,     // Root OID of this table
        int min_visible_column,        // Range of columns to expose for queries
        int max_visible_column,
        std::vector<int> index_types): // Types of the index columns
    _name(name),
    _oidlen(64),
    _handler_reg(NULL)
  {
    read_objid(tbl_oid.c_str(), _tbl_oid, &_oidlen);
    _table = netsnmp_tdata_create_table(_name.c_str(), 0);
    _table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    
    _table_info->min_column = min_visible_column;
    _table_info->max_column = max_visible_column;

    // Set each column index on both underlying objects.
    for (std::vector<int>::iterator ii = index_types.begin();
         ii != index_types.end();
         ii++)
    {
      netsnmp_tdata_add_index(_table, *ii);
      netsnmp_table_helper_add_index(_table_info, *ii);
    }
 
    TRC_INFO("Registering SNMP table %s", _name.c_str());
    _handler_reg = netsnmp_create_handler_registration(_name.c_str(),
                                                       Table::netsnmp_table_handler_fn,
                                                       _tbl_oid,
                                                       _oidlen,
                                                       HANDLER_CAN_RONLY | HANDLER_CAN_GETBULK);

    netsnmp_tdata_register(_handler_reg,
                           _table,
                           _table_info);
  }

  virtual ~Table()
  {
    if (_handler_reg)
    {
      netsnmp_unregister_handler(_handler_reg);
    }
    snmp_free_varbind(_table->indexes_template);
    snmp_free_varbind(_table_info->indexes);
    netsnmp_tdata_delete_table(_table);
    SNMP_FREE(_table_info);
  }

  // Add a Row into the underlying table.
  void add(T* row)
  {
    netsnmp_tdata_add_row(_table, row->get_netsnmp_row());
  };

  // Remove a Row from the underlying table.
  void remove(T* row)
  {
    netsnmp_tdata_remove_row(_table, row->get_netsnmp_row());
  };

protected:
  std::string _name;
  oid _tbl_oid[64];
  size_t _oidlen;
  netsnmp_handler_registration* _handler_reg;
  netsnmp_table_registration_info* _table_info;
  netsnmp_tdata* _table;
private:
  // netsnmp handler function (of type Netsnmp_Node_Handler). Called for each SNMP request on a table,
  // and maps the row and column to a value.
  static int netsnmp_table_handler_fn(netsnmp_mib_handler *handler,
                                      netsnmp_handler_registration *reginfo,
                                      netsnmp_agent_request_info *reqinfo,
                                      netsnmp_request_info *requests)
  {
    std::map<netsnmp_tdata_row*, SNMP::ColumnData> cache;
    char buf[64];

    TRC_DEBUG("Starting handling batch of SNMP requests");

    for (; requests != NULL; requests = requests->next)
    {
      snprint_objid(buf, sizeof(buf),
                    requests->requestvb->name, requests->requestvb->name_length);
      
      if (requests->processed)
      {
        continue;
      }

      netsnmp_tdata_row* row = netsnmp_tdata_extract_row(requests);
      netsnmp_table_request_info* table_info = netsnmp_extract_table_info(requests);

      if (!row || !table_info || !row->data)
      {
        // This should not have been passed through to this handler
        TRC_WARNING("Request for nonexistent row - OID %s", buf);
        return SNMP_ERR_NOSUCHNAME;
      }

      // Map back to the original SNMP::Row object.
      SNMP::Row* data = static_cast<SNMP::Row*>(row->data);

      // We need to get information a row at a time, and remember it - this avoids us reading column
      // 1, and having the data change before we query column 2
      if (cache.find(row) == cache.end())
      {
        SNMP::ColumnData cd = data->get_columns();
        cache[row] = cd;
      }
      
      if (cache[row][table_info->colnum].size != 0)
      {
        snmp_set_var_typed_value(requests->requestvb,
                                 cache[row][table_info->colnum].type,
                                 cache[row][table_info->colnum].value,
                                 cache[row][table_info->colnum].size);
      }
      else
      {
        TRC_WARNING("No value for OID %s", buf);
        return SNMP_ERR_NOSUCHNAME;
      }
    }

    TRC_DEBUG("Finished handling batch of SNMP requests");
    return SNMP_ERR_NOERROR;
  }
};

// Base ManagedTable, which manages the ownership of rows (whereas ordinary Tables expect Row
// objects to be owned by the user).
template<class TRow, class TRowKey> class ManagedTable : public Table<TRow>
{
public:
  ManagedTable(std::string name,
               std::string tbl_oid,
               int min_visible_column,
               int max_visible_column,
               std::vector<int> index_types):
    Table<TRow>(name, tbl_oid, min_visible_column, max_visible_column, index_types) {}

  // Upon destruction, release all the rows we're managing.
  virtual ~ManagedTable()
  {
    for (typename std::map<TRowKey, TRow*>::iterator ii = _map.begin();
         ii != _map.end();
         ii++)
    {
      // Can't just call into remove() here because it would modify the map we're iterating over
      TRow* row = ii->second;
      Table<TRow>::remove(row);
      delete row;
    }
  }

  void add(TRowKey key, TRow* row)
  {
    std::pair<TRowKey, TRow*> new_entry(key, row);
    _map.insert(new_entry);

    Table<TRow>::add(row);
  }

  // Creates the row keyed off `key`.
  void add(TRowKey key)
  {
    TRow* row = new_row(key);
    if (row != NULL)
    {
      add(key, row);
    }
    else
    {
      TRC_ERROR("Failed to add row to table %s", this->_name.c_str());
    }
  }

  // Returns the row keyed off `key`, creating it if it does not already exist.
  TRow* get(TRowKey key)
  {
    if (_map.find(key) == _map.end())
    {
      add(key);
    }

    return _map.at(key);
  }

  // Deletes the row keyed off `key`.
  void remove(TRowKey key)
  {
    if (_map.find(key) != _map.end())
    {
      TRow* row = _map.at(key);
      _map.erase(key);
      Table<TRow>::remove(row);
      delete row;
    }
  };

protected:
  // Subclasses need to specify how to create particular types of row, so that add() nd get() can
  // create them automatically.
  virtual TRow* new_row(TRowKey key) = 0;

  void add(TRow* row) { Table<TRow>::add(row); };
  void remove(TRow* row) { Table<TRow>::remove(row); };
  std::map<TRowKey, TRow*> _map;
};

} // namespace SNMP
#endif
