/**
 * @file snmp_infinite_base_table.h
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
 * software and licensed und er the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include <vector>
#include <map>
#include <string>
#include <atomic>

#include "logger.h"
#include "snmp_row.h"
#include "snmp_internal/snmp_includes.h"


#ifndef SNMP_INFINITE_BASE_TABLE_H
#define SNMP_INFINITE_BASE_TABLE_H

namespace SNMP
{

class InfiniteBaseTable
{
protected:
  InfiniteBaseTable(std::string name,
                    std::string oid,
                    uint32_t max_row,
                    uint32_t max_column);

  virtual ~InfiniteBaseTable();
  virtual Value get_value(std::string, uint32_t, uint32_t, timespec) = 0;

private:
  static const uint32_t MAX_TAG_LEN = 16;
  static const ssize_t SCRATCH_BUF_LEN = 128;

  std::string _name;
  oid _tbl_oid[SCRATCH_BUF_LEN];
  size_t _tbl_oid_len;
  const uint32_t _max_row;
  const uint32_t _max_column;
  netsnmp_handler_registration* _handler_reg;
  uint32_t ROOT_OID_LEN;

  static int static_netsnmp_table_handler_fn(netsnmp_mib_handler *handler,
                                             netsnmp_handler_registration *reginfo,
                                             netsnmp_agent_request_info *reqinfo,
                                             netsnmp_request_info *requests);

  int netsnmp_table_handler_fn(netsnmp_mib_handler *handler,
                               netsnmp_handler_registration *reginfo,
                               netsnmp_agent_request_info *reqinfo,
                               netsnmp_request_info *requests);

  bool validate_oid(const oid* oid,
                    const uint32_t oid_len);

  void parse_oid(const oid* oid,
                 const uint32_t oid_len,
                 std::string& tag,
                 uint32_t& row,
                 uint32_t& column);

  void find_next_oid(const oid* req_oid,
                     const uint32_t& req_oid_len,
                     std::unique_ptr<oid[]>& new_oid,
                     uint32_t& new_oid_len);
};

}

#endif
