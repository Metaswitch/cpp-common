/**
 * @file snmp_ip_time_based_counter_table.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2016 Metaswitch Networks Ltd
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

#include <atomic>

#include "snmp_internal/snmp_table.h"
#include "snmp_internal/snmp_includes.h"

#include "current_and_previous.h"
#include "snmp_types.h"
#include "snmp_ip_row.h"
#include "snmp_ip_time_based_counter_table.h"

namespace SNMP
{

// Forward declaration to break circular references.
class IPTimeBasedCounterTableImpl;

// A row in the IP time based count table.
class IPTimeBasedCounterRow : public IPRow
{
public:
  // IPv4 constructor
  IPTimeBasedCounterRow(struct in_addr addr,
                         const std::string& ip_str,
                         TimePeriodIndexes time_period,
                         IPTimeBasedCounterTableImpl* table) :
    IPRow(addr), _table(table), _ip_str(ip_str), _time_period(time_period)
  {
    netsnmp_tdata_row_add_index(_row,
                                ASN_INTEGER,
                                &_time_period,
                                sizeof(int));
  }

  // IPv6 constructor
  IPTimeBasedCounterRow(struct in6_addr addr,
                         const std::string& ip_str,
                         TimePeriodIndexes time_period,
                         IPTimeBasedCounterTableImpl* table) :
    IPRow(addr), _table(table), _ip_str(ip_str), _time_period(time_period)
  {
    netsnmp_tdata_row_add_index(_row,
                                ASN_INTEGER,
                                &_time_period,
                                sizeof(int));
  }

  virtual ~IPTimeBasedCounterRow() {}

  // Get column data. Implemented below (again to break circular references).
  ColumnData get_columns();

private:
  // Pointer to the parent table, used to retrieve counts when queried by
  // netsnmp.
  IPTimeBasedCounterTableImpl* _table;

  // The IP address in string form. This is needed to retrieve entries from the
  // parent table.
  std::string _ip_str;

  // The time period this row refers to.
  TimePeriodIndexes _time_period;
};

// This is the index used to identify rows in the ManagedTable.
typedef std::pair<std::string, TimePeriodIndexes> IPTimeBasedCounterIndex;

// Implementation of the table.
class IPTimeBasedCounterTableImpl : public IPTimeBasedCounterTable,
                                     public ManagedTable<IPTimeBasedCounterRow, IPTimeBasedCounterIndex>
{
public:
  IPTimeBasedCounterTableImpl(std::string name, std::string tbl_oid) :
    ManagedTable<IPTimeBasedCounterRow, IPTimeBasedCounterIndex>(
      name, tbl_oid, 4, 4, { ASN_INTEGER, ASN_OCTET_STR, ASN_INTEGER })
  {
    pthread_rwlock_init(&_table_lock, NULL);
  }

  ~IPTimeBasedCounterTableImpl()
  {
    pthread_rwlock_destroy(&_table_lock);

    for(std::map<std::string, IPEntry*>::iterator it = _counters_by_ip.begin();
        it != _counters_by_ip.end();
        ++it)
    {
      delete it->second; it->second = NULL;
    }
  }

  void add_ip(const std::string& ip)
  {
    // Add an IP address. We might be about to mutate the counter map, so grab
    // the write lock.
    pthread_rwlock_wrlock(&_table_lock);

    std::map<std::string, uint32_t>::iterator ref_entry = _ref_count_by_ip.find(ip);

    if (ref_entry == _ref_count_by_ip.end())
    {
      _ref_count_by_ip[ip] = 1;

      std::map<std::string, IPEntry*>::iterator entry = _counters_by_ip.find(ip);

      if (entry == _counters_by_ip.end())
      {
        // IP address does not already exist - add an entry in the counts map and
        // the associated SNMP rows.
        TRC_DEBUG("Adding IP rows for: %s", ip.c_str());

        _counters_by_ip[ip] = new IPEntry();
        add(std::make_pair(ip, TimePeriodIndexes::scopePrevious5SecondPeriod));
        add(std::make_pair(ip, TimePeriodIndexes::scopeCurrent5MinutePeriod));
        add(std::make_pair(ip, TimePeriodIndexes::scopePrevious5MinutePeriod));
      }
      else
      {
        TRC_ERROR("Entry for %s doesn't exist in reference table, but does exist in count table",
                  ip.c_str());
      }
    }
    else
    {
      ref_entry->second++;
    }

    pthread_rwlock_unlock(&_table_lock);
  }

  void remove_ip(const std::string& ip)
  {
    // Remove an IP address. We might be about to mutate the counter map, so
    // grab the write lock.
    pthread_rwlock_wrlock(&_table_lock);

    std::map<std::string, uint32_t>::iterator ref_entry = _ref_count_by_ip.find(ip);

    if (ref_entry == _ref_count_by_ip.end())
    {
      TRC_ERROR("Attempted to delete row for %s which isn't in the reference table",
                ip.c_str());
    }
    else
    {
      ref_entry->second --;

      // If we have removed the last reference to this entry, remove it from the
      // counts table.
      if (ref_entry->second == 0)
      {
        std::map<std::string, IPEntry*>::iterator entry = _counters_by_ip.find(ip);

        if (entry != _counters_by_ip.end())
        {
          // IP address already exists - remove the entry from the counts map and
          // delete the associated SNMP rows.
          TRC_DEBUG("Removing IP rows for %s", ip.c_str());

          delete entry->second; entry->second = NULL;
          _counters_by_ip.erase(entry);
          remove(std::make_pair(ip, TimePeriodIndexes::scopePrevious5SecondPeriod));
          remove(std::make_pair(ip, TimePeriodIndexes::scopeCurrent5MinutePeriod));
          remove(std::make_pair(ip, TimePeriodIndexes::scopePrevious5MinutePeriod));
        }
        else
        {
          TRC_ERROR("Entry for %s exists in reference table, but not the count table",
                    ip.c_str());
        }
      }
    }

    pthread_rwlock_unlock(&_table_lock);
  }

  void increment(const std::string& ip)
  {
    // Increment the count for the specified IP. This cannot mutate the counts
    // map (only the counts stored within it, which are atomic), so we only need
    // the read lock.
    pthread_rwlock_rdlock(&_table_lock);

    std::map<std::string, IPEntry*>::iterator entry = _counters_by_ip.find(ip);
    if (entry != _counters_by_ip.end())
    {
      TRC_DEBUG("Incrementing counter for %s", ip.c_str());
      entry->second->five_sec.get_current()->counter++;
      entry->second->five_min.get_current()->counter++;
    }

    pthread_rwlock_unlock(&_table_lock);
  }

  uint32_t get_count(const std::string& ip, TimePeriodIndexes time_period)
  {
    TRC_DEBUG("Get count for IP: %s, time period: %d", ip.c_str(), time_period);

    uint32_t count = 0;

    // Reading a count cannot mutate the counts map (only the counts stored
    // within it which are atomic), so we only need the read lock.
    pthread_rwlock_rdlock(&_table_lock);

    std::map<std::string, IPEntry*>::iterator entry = _counters_by_ip.find(ip);
    if (entry != _counters_by_ip.end())
    {
      switch (time_period)
      {
      case TimePeriodIndexes::scopePrevious5SecondPeriod:
        count = entry->second->five_sec.get_previous()->counter;
        break;

      case TimePeriodIndexes::scopeCurrent5MinutePeriod:
        count = entry->second->five_min.get_current()->counter;
        break;

      case TimePeriodIndexes::scopePrevious5MinutePeriod:
        count = entry->second->five_min.get_previous()->counter;
        break;

      default:
        // LCOV_EXCL_START
        TRC_ERROR("Invalid time period requested: %d", time_period);
        count = 0;
        break;
        // LCOV_EXCL_STOP
      }
    }

    pthread_rwlock_unlock(&_table_lock);

    TRC_DEBUG("Counter is %d", count);
    return count;
  }

private:

  IPTimeBasedCounterRow* new_row(IPTimeBasedCounterIndex index)
  {
    std::string& ip = index.first;
    TimePeriodIndexes time_period = index.second;
    TRC_DEBUG("Create new SNMP row for IP: %s, time period: %d", ip.c_str(), time_period);

    struct in_addr  v4;
    struct in6_addr v6;

    // Convert the string into a type (IPv4 or IPv6) and a sequence of bytes
    if (inet_pton(AF_INET, ip.c_str(), &v4) == 1)
    {
      return new IPTimeBasedCounterRow(v4, ip, time_period, this);
    }
    else if (inet_pton(AF_INET6, ip.c_str(), &v6) == 1)
    {
      return new IPTimeBasedCounterRow(v6, ip, time_period, this);
    }
    else
    {
      TRC_ERROR("Could not parse %s as an IPv4 or IPv6 address", ip.c_str());
      return NULL;
    }
  }

  // A simple counter - this is needed so we can construct a CurrentAndPrevious
  // (which requires the underlying type to have a reset method).
  struct Counter
  {
    std::atomic_uint_fast32_t counter;

    void reset(uint64_t time_periodstart, Counter* previous = NULL)
    {
      counter = 0;
    }
  };

  // The current/previous 5 second and 5 minute counts for a single IP address.
  struct IPEntry
  {
    IPEntry() : five_sec(5 * 1000), five_min(5 * 60 * 1000) {}
    CurrentAndPrevious<Counter> five_sec;
    CurrentAndPrevious<Counter> five_min;
  };

  // A container of counts indexed by IP address. This can be accessed on
  // multiple threads, and so is protected by _table_lock.
  std::map<std::string, IPEntry*> _counters_by_ip;

  // A reference count for each IP address, keeping track of how many times
  // it's been added and removed. This is protected by _table_lock.
  std::map<std::string, uint32_t> _ref_count_by_ip;

  pthread_rwlock_t _table_lock;
};


IPTimeBasedCounterTable* IPTimeBasedCounterTable::create(std::string name,
                                                         std::string oid)
{
  return new IPTimeBasedCounterTableImpl(name, oid);
}


ColumnData IPTimeBasedCounterRow::get_columns()
{
  TRC_DEBUG("Columns requested for row: IP: %s time period: %d", _ip_str.c_str(), _time_period);

  ColumnData ret;
  // IP address
  ret[1] = Value::integer(_addr_type);
  ret[2] = Value(ASN_OCTET_STR, (unsigned char*)&_addr, _addr_len);
  // Time period
  ret[3] = Value::integer(_time_period);
  // Count
  ret[4] = Value::uint(_table->get_count(_ip_str, _time_period));
  return ret;
}

}
