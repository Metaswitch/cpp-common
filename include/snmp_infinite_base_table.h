/**
 * @file snmp_infinite_base_table.h
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
