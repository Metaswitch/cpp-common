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

#include <string>

#include "snmp_statistics_structures.h"
#include "snmp_infinite_timer_count_table.h"
#include "timer_counter.h"
#include "snmp_internal/snmp_includes.h"
#include "snmp_row.h"

#include "log.h"
#include "logger.h"

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

      netsnmp_register_handler(_handler_reg);
    }

    virtual ~InfiniteTimerCountTableImpl()
    {
      if (_handler_reg)
      {
        netsnmp_unregister_handler(_handler_reg);
      }
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
    char buf[64];
    size_t _oidlen;
    int ROOT_OID_LEN = 7; // Currently "1.2.826.0.1.1578918.999";
    unsigned long new_oid[128];
    unsigned long* new_oid_p = &new_oid[0];
    netsnmp_handler_registration* _handler_reg;
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
      TRC_INFO("Starting handling batch of SNMP requests");

      unsigned long new_oid_len = 0;

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
        std::string tag;
        std::vector<int> identifier;
        SimpleStatistics stats;
        int request_type = reqinfo->mode;
        netsnmp_variable_list* var = requests->requestvb;
        Value result;

        // Get the time we will process this request at
        struct timespec now;
        clock_gettime(CLOCK_REALTIME_COARSE, &now);

        // Populate the values for tag and identifier - e.g. tag = "CALL",
        // identifier = <2, 1>
        parse_request(requests->requestvb->name, requests->requestvb->name_length, &tag, &identifier);

        TRC_DEBUG("Parse request with tag: %s", tag.c_str());

        // Update the identifier based on the request type, this gives us a
        // valid, logical OID that we will query
        bool found = update_identifier(request_type, &identifier);

        if (!found && request_type == MODE_GET)
        {
          TRC_INFO("Invalid GET request");
          return SNMP_ERR_NOSUCHNAME;
        }

        if (!found && request_type == MODE_GETNEXT)
        {
          TRC_INFO("This request goes beyond the table");
          // new_oid variable is pointing to the space after the tag
          new_oid_p--;
          *new_oid_p = *(new_oid_p) + 1;
          new_oid_p++;
          snmp_set_var_objid(var,
                             new_oid_p,
                             new_oid_len);

          snmp_set_var_typed_value(var,
                                   result.type,
                                   result.value,
                                   result.size);

          return SNMP_ERR_NOERROR;
        }


        // Update and obtain the relevants statistics structure
        _timer_counters[tag].get_values(identifier.back(), now, &stats);

        TRC_DEBUG("Have got statistics structure");

        // Calculate the appropriate value - i.e. avg, var, hwm or lwm
        result = get_value(&stats, tag, identifier, now);

        if (*result.value == -1)
        {
          TRC_INFO("Failed to get value from the structure");
          return SNMP_ERR_NOSUCHNAME;
        }

        for (int i = 0; i < (int)(identifier.size()); i++)
        {
          *new_oid_p = (unsigned long)(identifier.at(i));
          new_oid_p++;
          new_oid_len++;
        }

        new_oid_p -= new_oid_len;

        snmp_set_var_objid(var,
                           new_oid_p,
                           new_oid_len);

        snmp_set_var_typed_value(var,
                                 result.type,
                                 result.value,
                                 result.size);
      }

      TRC_INFO("Finished handling batch of SNMP requests");
      new_oid_len = 0;
      return SNMP_ERR_NOERROR;
    }

    void parse_request(oid* oid,
                       unsigned long oid_len,
                       std::string* tag,
                       std::vector<int>* identifier)
    {
      for (int ii = 0; ii<(int)(oid_len); ii++)
      {
        TRC_INFO("Element at %d is %u", ii, *(oid+ii));
      }

      for (int ii = 0; ii<(int)(ROOT_OID_LEN); ii++)
      {
        *new_oid_p = *oid;
        oid++;
        new_oid_p++;
        new_oid_len++;
      }
      int length_of_tag = *oid;
      *new_oid_p = *oid;
      oid++;
      new_oid_p++;
      new_oid_len++;

      char tag_buff[128];

      for (int ii = 0; ii < length_of_tag; ii++)
      {
        tag_buff[ii] = *oid;
        *new_oid_p = *oid;
        oid++;
        new_oid_p++;
        new_oid_len++;
      }
      tag_buff[length_of_tag] = '\0';
      *tag = std::string(tag_buff);

      int remainining_steps = oid_len - ROOT_OID_LEN - length_of_tag - 1;

      for (int ii = 0; ii < remainining_steps; ii++)
      {
        identifier->push_back(*oid);
        oid++;
      }
    }

    bool update_identifier(int request_type, std::vector<int>* identifier)
    {
      TRC_INFO("Validating identifier");
      switch (request_type)
      {
      case MODE_GET:
        // As it is a GET, the request must match exactly with an OID we are
        // managing. Current implementation is 4 columns, and 3 rows.
        if (identifier->size() != 2 ||
            identifier->front() > 5 ||
            identifier->front() < 2 ||
            identifier->back() > 3 ||
            identifier->back() < 1)
        {
          // The identifer is not long enough to specify a value or is out of
          // the bounds of the table
          return false;
        }
        break;

      case MODE_GETNEXT:
        // As it is a GETNEXT, we update the identifier with the next logical
        // value
        identifier->push_back(0);
        identifier->push_back(0);
        if (identifier->size() > 2)
        {
          identifier->resize(2);
        }
        identifier->at(1)++;
        if (identifier->at(1) > 3)
        {
          identifier->at(0)++;
          identifier->at(1) = 1;
        }
        if (identifier->at(0) < 2)
        {
          identifier->at(0) = 2;
        }
        if (identifier->at(0) > 5)
        {
          return false;
        }
        break;

      default:
        snmp_log(LOG_ERR, "problem encountered in Clearwater handler: unsupported mode %d", request_type);
        return false;
      }
      TRC_INFO("It now has a valid form: <%d, %d>", identifier->at(0), identifier->at(1));
      return true;
    }

    Value get_value(SimpleStatistics* data,
                    std::string tag,
                    std::vector<int> identifier,
                    timespec now)
    {
      switch (identifier.front())
      {
        case 2:
          // Calculate the average
          return Value::uint(data->average);
        case 3:
          // Calculate the variance
          return Value::uint(data->variance);
        case 4:
          // Get the HWM
          return Value::uint(data->hwm);
        case 5:
          // Get the LWM
          return Value::uint(data->lwm);
      }
      return Value::integer(-1);
    }

  };


  InfiniteTimerCountTable* InfiniteTimerCountTable::create(std::string name, std::string oid)
  {
    return new InfiniteTimerCountTableImpl(name, oid);
  }
}
