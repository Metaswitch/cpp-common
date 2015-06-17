/**
 * @file snmp_latency_table.cpp
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

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "snmp_latency_table.h"
#include <vector>
#include <map>
#include <string>
#include <tuple>

namespace SNMP
{

ColumnData AccumulatorRow::get_columns()
{
  uint32_t sum = accumulated.sum;
  uint32_t sumsq = accumulated.sqsum;
  uint32_t count = accumulated.count;
  uint32_t avg = sum/std::max(count, 1u);
  uint32_t variance = (sumsq/std::max(count, 1u)) - avg;
  
  // Construct and return a ColumnData with the appropriate values
  ColumnData ret;
  ret[1] = Value::integer(index);
  ret[2] = Value::uint(count);
  ret[3] = Value::uint(avg);
  ret[4] = Value::uint(variance);
  ret[5] = Value::uint(accumulated.lwm);
  ret[6] = Value::uint(accumulated.hwm);
  return ret;
}

void AccumulatorRow::accumulate(uint32_t latency)
{
  accumulated.count++;
  accumulated.sum += latency;
  accumulated.sqsum += (latency * latency);
  if (latency > accumulated.hwm)
  {
    accumulated.hwm = latency;
  }

  if ((latency < accumulated.lwm) || (accumulated.lwm == 0))
  {
    accumulated.lwm = latency;
  }
};

void AccumulatorRow::update_time()
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC_COARSE, &now);

  // The 'tick' signifies how many five-second windows have passed - if it's odd, we should read
  // from fiveseconds_odd and fiveseconds_even. If it's even, vice-versa.
  uint32_t new_tick = (now.tv_sec / interval);

  if (new_tick > tick)
  {
    if ((new_tick % 2) == 0)
    {
    }
    else
    {
    }
  }
  tick = new_tick;
}

}
