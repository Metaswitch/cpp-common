/**
 * @file snmp_table.cpp
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

#include <map>

#include "snmp_includes.h"
#include "snmp_table.h"
#include "log.h"

// General handler function, which extracts the SNMPRowGroup from the request, calls its
// get_columns() method, and plumbs the answer back into net-snmp.
int netsnmp_table_handler_fn(netsnmp_mib_handler *handler,
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
