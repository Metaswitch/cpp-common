/**
 * @file snmp_infinite_chronos_table.cpp
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

class InfiniteTimerCountTable
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

    TRC_INFO("Starting handling batch of SNMP requests");

    for (; requests != NULL; requests = requests->next)
    {
      snprint_objid(buf, sizeof(buf),
                    requests->requestvb->name, requests->requestvb->name_length);
      TRC_INFO("Handling SNMP request for OID %s", buf);

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
      TRC_DEBUG("Got %d columns for row %p\n", cache[row].size(), row);

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

    TRC_INFO("Finished handling batch of SNMP requests");
    return SNMP_ERR_NOERROR;
  }
};



  /********************************/
  /*  Methods for TimerHandler    */
  /********************************/

  void increment(std::string tag)
  {
    _timer_counters[tag].increment();
  }

  void decrement(std::string tag)
  {
    _timer_counter[tag].decrement();
  }

  std::map<std::string, TimerCounter> _timer_counters;
}

