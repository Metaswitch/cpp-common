/**
 * @file snmp_infinite_scalar_table.cpp
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

#include <string>
#include <algorithm>
#include <memory>
#include <mutex>

#include "snmp_infinite_scalar_table.h"
#include "snmp_row.h"
#include "snmp_infinite_base_table.h"

#include "log.h"
#include "logger.h"


namespace SNMP
{
  class InfiniteScalarTableImpl : public InfiniteScalarTable, public InfiniteBaseTable
  {
  public:
    InfiniteScalarTableImpl(std::string name, // Name of this table, for logging
                            std::string tbl_oid):// Root OID of this table
    InfiniteBaseTable(name, tbl_oid, max_row, max_column){}

    virtual ~InfiniteScalarTableImpl(){};
    
    void increment(std::string tag, uint32_t count)
    {
      _mutex.lock();
      _scalar_counters[tag] += count;
      _mutex.unlock();
    }

    void decrement(std::string tag, uint32_t count)
    {
      // Ensure scalar value does not become negative
      _mutex.lock();
      if (_scalar_counters[tag] > count)
        {
          _scalar_counters[tag] -= count;
        }
      else
        {
          _scalar_counters[tag] = 0;
        }
      _mutex.unlock();
    }

  protected:
    static const uint32_t max_row = 1;
    static const uint32_t max_column = 2;
    std::map<std::string, uint32_t> _scalar_counters;
    std::mutex _mutex;

  private:
    Value get_value(std::string tag,
                    uint32_t column,
                    uint32_t row,
                    timespec now)
    {
      Value result = Value::uint(0);

      if (column !=2 && row != 1)
      {
        // This should never happen - find_next_oid should police this.
        TRC_DEBUG("Internal MIB error - column %d is out of bounds",
                  column);
        return Value::uint(0);
      }

      _mutex.lock();
      if (_scalar_counters.count(tag) > 0)
      {
        result = Value::uint(_scalar_counters[tag]);
      }
      _mutex.unlock();

      TRC_DEBUG("Got value %u for tag %s cell (%d, %d)",
                *result.value, tag.c_str(), row, column);

      return result;
    }
  };

  InfiniteScalarTable* InfiniteScalarTable::create(std::string name, std::string oid)
  {
    return new InfiniteScalarTableImpl(name, oid);
  };
}
