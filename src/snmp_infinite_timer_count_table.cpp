/**
 * @file snmp_infinite_timer_count_table.cpp
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

#include "snmp_statistics_structures.h"
#include "snmp_infinite_timer_count_table.h"
#include "timer_counter.h"
#include "snmp_internal/snmp_includes.h"
#include "snmp_row.h"

#include "log.h"

namespace SNMP
{

class InfiniteTimerCountTableImpl : public InfiniteTimerCountTable
{
public:
  InfiniteTimerCountTableImpl(std::string name, // Name of this table, for logging
                             std::string tbl_oid):  // Root OID of this table
                             _name(name),
                             _oidlen(64),
                             _handler_reg(NULL)
  {
    read_objid(tbl_oid.c_str(), _tbl_oid, &_oidlen);


    TRC_INFO("Registering SNMP table %s", _name.c_str());
    _handler_reg = netsnmp_create_handler_registration(_name.c_str(),
                                                       InfiniteTimerCountTableImpl::static_netsnmp_table_handler_fn,
                                                       _tbl_oid,
                                                       _oidlen,
                                                       HANDLER_CAN_RONLY | HANDLER_CAN_GETBULK);
    _handler_reg->handler->myvoid = this;

    netsnmp_tdata_register(_handler_reg,
                           _table,
                           _table_info);
  }

  virtual ~InfiniteTimerCountTableImpl()
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

  /********************************/
  /*  Methods for TimerHandler    */
  /********************************/

  void increment(std::string tag)
  {
    _timer_counters[tag].increment();
  }

  void decrement(std::string tag)
  {
    _timer_counters[tag].decrement();
  }

  std::map<std::string, TimerCounter> _timer_counters;

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
  static int static_netsnmp_table_handler_fn(netsnmp_mib_handler *handler,
                               netsnmp_handler_registration *reginfo,
                               netsnmp_agent_request_info *reqinfo,
                               netsnmp_request_info *requests)
  {
    return (static_cast<InfiniteTimerCountTableImpl*>(handler->myvoid))->netsnmp_table_handler_fn(handler,
                                                                                        reginfo,
                                                                                        reqinfo,
                                                                                        requests);

  }

  int netsnmp_table_handler_fn(netsnmp_mib_handler *handler,
                               netsnmp_handler_registration *reginfo,
                               netsnmp_agent_request_info *reqinfo,
                               netsnmp_request_info *requests)
  {
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

      // We have a request that we need to parse
      //
      //
      // Suppose we have parsed it
      std::string tag = "CALL";
      std::vector<int> identifier(2,2);
      std::string request_type = "GET";

      if (request_type == "GET")
      {
        // As it is a GET, the request must match exactly with an OID we are
        // managing. Current implementation is 4 columns, and 3 rows.
        if (identifier.size() != 2 ||
            identifier.front() > 5 ||
            identifier.front() < 2 ||
            identifier.back() > 3 ||
            identifier.back() < 1)
        {
          // The identifer is not long enough to specify a value or is out of
          // the bounds of the table
          TRC_WARNING("No value found - OID %s", buf);
          return SNMP_ERR_NOSUCHNAME;
        }

        // Get the time we will process this request at
        struct timespec now;
        clock_gettime(CLOCK_REALTIME_COARSE, &now);

        _timer_counters[tag].update_values(now);
        ContinuousStatistics* values;

        values = _timer_counters[tag].get_values(identifier.back(), now);

        TRC_DEBUG("%i", values->hwm.load());

        switch (identifier.front())
        {
          // Calculate the average
          case 1:
            break;
          // Calculate the variance
          case 2:
            break;
          // Get the HWM
          case 3:
            break;
          // Get the LWM
          case 4:
            break;
        }
      }

      else if (request_type == "GET_NEXT")
      {
        // As it is a GETNEXT, e request must match exactly with an OID we are
        // managing. Current implementation is 4 columns, and 3 rows.
        if (identifier.size() != 2 ||
            identifier.front() > 5 ||
            identifier.front() < 2 ||
            identifier.back() > 3 ||
            identifier.back() < 1)
        {
          // The identifer is not long enough to specify a value or is out of
          // the bounds of the table
          TRC_WARNING("No value found - OID %s", buf);
          return SNMP_ERR_NOSUCHNAME;
        }

        // Get the time we will process this request at
        struct timespec now;
        clock_gettime(CLOCK_REALTIME_COARSE, &now);

        _timer_counters[tag].update_values(now);
        ContinuousStatistics* data;

        data = _timer_counters[tag].get_values(identifier.back(), now);
        uint32_t interval_ms = _timer_counters[tag].get_interval_ms(identifier.back());

        TRC_DEBUG("%i", data->hwm.load());

        Value result;

        uint64_t time_period_start_ms = data->time_period_start_ms.load();
        uint64_t time_period_end_ms = ((time_period_start_ms + interval_ms) / interval_ms) * interval_ms;
        uint64_t time_now_ms = (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
        uint64_t time_comes_first_ms = std::min(time_period_end_ms, time_now_ms);
        uint64_t period_count = (time_comes_first_ms - time_period_start_ms);

        uint64_t sum = data->sum.load();
        uint64_t sqsum = data->sqsum.load();

        switch (identifier.front())
        {
          // Calculate the average
          case 1:
            result = Value::uint(sum / period_count);
            break;
          // Calculate the variance
          case 2:
            result = Value::uint(((sqsum * period_count) - (sum * sum)) /
                                 (period_count * period_count));
            break;
          // Get the HWM
          case 3:
            result = Value::uint(data->hwm.load());
            break;
          // Get the LWM
          case 4:
            result = Value::uint(data->lwm.load());
            break;
        }

        snmp_set_var_typed_value(requests->requestvb,
                                 result.type,
                                 result.value,
                                 result.size);

      }



    }

    TRC_INFO("Finished handling batch of SNMP requests");
    return SNMP_ERR_NOERROR;
  }
};


InfiniteTimerCountTable* InfiniteTimerCountTable::create(std::string name, std::string oid)
{
  return new InfiniteTimerCountTableImpl(name, oid);
}
}
