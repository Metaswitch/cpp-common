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
#include <atomic>

SNMPLatencyRowGroup::SNMPLatencyRowGroup(int ignored):
  SNMPRowGroup(),
{
  // Create the underlying netsnmp row objects
  five_second_row = new Row(1, 5, static_cast<SNMPRowGroup*>(this));
  five_minute_row = new Row(1, 300, static_cast<SNMPRowGroup*>(this));
  _rows.push_back(five_second_row->netsnmp_row);
  _rows.push_back(five_minute_row->netsnmp_row);
}

ColumnData SNMPLatencyRowGroup::get_columns(netsnmp_tdata_row* netsnmp_row)
{
  Row* row = NULL;

  // Read from the row that's not currently being updated.
  if (netsnmp_row == five_second_row->netsnmp_row)
  {
    row = five_second_row;
  }
  else
  {
    row = five_minute_row;
  }

  LatencyValues* values = row->get_reading_ptr();
  uint32_t sum = values->sum.load();
  uint32_t sumsq = values->sqsum.load();
  uint32_t count = values->count.load();
  uint32_t avg = sum/std::max(count, 1u);
  uint32_t variance = (sumsq/std::max(count, 1u)) - avg;
  
  // Construct and return a ColumnData with the appropriate values
  ColumnData ret;
  ret[1] = SNMPValue::integer(row->index);
  ret[2] = SNMPValue::uint(count);
  ret[3] = SNMPValue::uint(avg);
  ret[4] = SNMPValue::uint(variance);
  ret[5] = SNMPValue::uint(values->lwm.load());
  ret[6] = SNMPValue::uint(values->hwm.load());
  return ret;
}
void SNMPLatencyRowGroup::accumulate(uint32_t latency)
{
  update_internal(five_seconds_row->get_writing_ptr(), latency);
  update_internal(five_minutes_row->get_writing_ptr(), latency);
}

void SNMPLatencyRowGroup::update_internal(LatencyValues* val, uint32_t latency)
{
  val->count++;
  val->sum.fetch_add(latency);
  val->sqsum.fetch_add(latency*latency);
  uint32_t hwm_seen, lwm_seen;
  do
  {
    hwm_seen = val->hwm.load();
    if (latency <= hwm_seen)
    {
      break;
    }
  } while (!val->hwm.compare_exchange_weak(hwm_seen, latency));

  do
  {
    lwm_seen = val->lwm.load();
    if ((lwm_seen > 0) && (latency >= lwm_seen))
    {
      break;
    }
  } while (!val->lwm.compare_exchange_weak(lwm_seen, latency));

};

void SNMPLatencyRowGroup::Row::zero_struct(LatencyValues& s)
{
  s.count.store(0);
  s.sum.store(0);
  s.sqsum.store(0);
  s.hwm.store(0);
  s.lwm.store(0);
}

void SNMPLatencyRowGroup::Row::update_time()
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
      reading = &even;
      writing = &odd;
    }
    else
    {
      reading = &odd;
      writing = &even;
    }
    zero_struct(*writing);
  }

  tick = new_tick;
}


