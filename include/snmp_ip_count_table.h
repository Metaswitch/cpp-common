/**
 * @file snmp_ip_count_table.h
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <vector>
#include <map>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "logger.h"
#include "snmp_row.h"
#include "snmp_ip_row.h"

#ifndef SNMP_IP_COUNT_TABLE_H
#define SNMP_IP_COUNT_TABLE_H

// This file contains the interface for tables which:
//   - are indexed by IP address and IP address type
//   - report a count for each IP address
//
// It also contains the interface for their rows.
//
// To use an IP count table, simply create one, call `get` on it to create appropriate rows, and
// call `increment` or `decrement` on those rows as necessary:
//
// SNMP::IPCountTable* xdm_cxns_table = SNMP::IPCountTable::create("connections_to_homer", ".1.2.3");
// xdm_cxns_table->get("10.0.0.1")->increment();
// xdm_cxns_table->get("10.0.0.2")->decrement();
//
// IPCountRow objects are automatically created when needed, but need to be explicitly deleted (with
// `remove`):
//
// xdm_cxns_table->remove("10.0.0.1");


namespace SNMP
{

// Row of counters indexed by RFC 2851 IP addresses
class IPCountRow : public IPRow
{
public:
  IPCountRow(struct in_addr addr);
  IPCountRow(struct in6_addr addr);

  uint32_t increment() { return ++_count; };
  uint32_t decrement() { return --_count; };

  ColumnData get_columns();

protected:
  uint32_t _count;
};

class IPCountTable
{
public:
  static IPCountTable* create(std::string name, std::string oid);
  virtual ~IPCountTable() {};
  virtual IPCountRow* get(std::string key) = 0;
  virtual void add(std::string key) = 0;
  virtual void remove(std::string key) = 0;
protected:
  IPCountTable() {};
};

}

#endif
