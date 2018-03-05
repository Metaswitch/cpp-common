/**
 * @file snmp_ip_timed_based_count_table.h
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef SNMP_IP_TIMED_BASED_COUNT_TABLE_H_
#define SNMP_IP_TIMED_BASED_COUNT_TABLE_H_

namespace SNMP
{

class IPTimeBasedCounterTable
{
public:
  virtual ~IPTimeBasedCounterTable() {};

  /// Create a new instance of the table.
  ///
  /// @param name - The name of the table.
  /// @param oid  - The OID subtree that the table lives within.
  ///
  /// @return     - The table instance.
  static IPTimeBasedCounterTable* create(std::string name, std::string oid);

  /// Add rows to the table for the specified IP address. If this IP already
  /// exists in the table, an additional reference count will be added for
  /// it.
  ///
  /// Calls to add_ip and remove_ip should be balanced.
  ///
  /// @param ip - The IP address to add. Must be a valid IPv4 or IPv6 IP
  ///             address.
  virtual void add_ip(const std::string& ip) = 0;

  /// Removes rows for the specified IP address from the table. If this IP has
  /// been added multiple times, this just removes one from the reference count.
  ///
  /// Calls to add_ip and remove_ip should be balanced.
  ///
  /// @param ip - The IP address to remove. Must be a valid IPv4 or IPv6 IP
  ///             address.
  virtual void remove_ip(const std::string& ip) = 0;

  /// Increment the count for the given IP. The IP address must have been
  /// previously added to the table by calling `add_ip`. If it has not, the
  /// increment is ignored.
  ///
  /// @param ip - The IP address to increment the stat for.
  virtual void increment(const std::string& ip) = 0;

protected:
  IPTimeBasedCounterTable() {};
};

}

#endif

