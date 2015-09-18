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
#include <algorithm>

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
      ROOT_OID_LEN = std::count(tbl_oid.begin(), tbl_oid.end(), '.');


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
    uint32_t ROOT_OID_LEN;
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

      for (; requests != NULL; requests = requests->next)
      {
        snprint_objid(buf, sizeof(buf),
                      requests->requestvb->name, requests->requestvb->name_length);
        TRC_INFO("Handling SNMP request for OID %s", buf);

        if (requests->processed)
        {
          continue;
        }

        unsigned long new_oid_a[64];
        unsigned long* new_oid = new_oid_a;
        uint32_t new_oid_len = 0;

        // We have a request that we need to parse
        std::string tag;
        std::vector<int> identifier;
        SimpleStatistics stats;
        int request_type = reqinfo->mode;
        netsnmp_variable_list* var = requests->requestvb;

        // We can set a default value of 0 unless we find a valid result
        Value result = Value::uint(0);

        // Get the time we will process this request at
        struct timespec now;
        clock_gettime(CLOCK_REALTIME_COARSE, &now);

        // Populate the values for tag and identifier - e.g. tag = "CALL",
        // identifier = <2, 1>
        parse_request(requests->requestvb->name,
                      requests->requestvb->name_length,
                      &tag,
                      &identifier,
                      new_oid,
                      new_oid_len);

        TRC_DEBUG("Parse request with tag: %s", tag.c_str());
        TRC_DEBUG("The current length of the new oid is: %u", new_oid_len);

        // Update the identifier based on the request type, this gives us a
        // valid, logical OID that we will query
        bool found = update_identifier(request_type, tag, &identifier, new_oid, new_oid_len);
        new_oid_len += 2;
        new_oid[ROOT_OID_LEN + 1 + tag.length()] = identifier.at(0);
        new_oid[ROOT_OID_LEN + 2 + tag.length()] = identifier.at(1);

        char buf1[64];
        snprint_objid(buf1, sizeof(buf1),
                      new_oid, new_oid_len);
        TRC_INFO("Returning SNMP request for OID %s", buf1);

        if (!found && request_type == MODE_GET)
        {
          TRC_INFO("Invalid GET request");
          return SNMP_ERR_NOSUCHNAME;
        }

        if (!found && request_type == MODE_GETNEXT)
        {
          TRC_INFO("This request goes beyond the table");
          new_oid[ROOT_OID_LEN + tag.length()]++;
          new_oid[ROOT_OID_LEN + tag.length() + 1] = 2;
          new_oid[ROOT_OID_LEN + tag.length() + 2] = 1;

          snmp_set_var_objid(var,
                             new_oid,
                             new_oid_len);

          snmp_set_var_typed_value(var,
                                   result.type,
                                   result.value,
                                   result.size);

          return SNMP_ERR_NOERROR;
        }

        // Update and obtain the relevants statistics structure
        _timer_counters[tag].get_statistics(identifier.back(), now, &stats);

        TRC_DEBUG("Have got statistics structure");

        // Calculate the appropriate value - i.e. avg, var, hwm or lwm
        result = get_value(&stats, tag, identifier, now);

        if (*result.value == -1)
        {
          TRC_INFO("Failed to get value from the structure");
          snmp_set_var_typed_value(var,
                                   result.type,
                                   result.value,
                                   result.size);
          return SNMP_ERR_NOSUCHNAME;
        }

        snmp_set_var_objid(var,
                           new_oid,
                           new_oid_len);

        snmp_set_var_typed_value(var,
                                 result.type,
                                 result.value,
                                 result.size);
      }

      TRC_INFO("Finished handling batch of SNMP requests");
      return SNMP_ERR_NOERROR;
    }

    void parse_request(oid* oid,
                       unsigned long oid_len,
                       std::string* tag,
                       std::vector<int>* identifier,
                       unsigned long* new_oid,
                       uint32_t &new_oid_len)
    {
      int length_of_tag = 0;
      char tag_buff[64];
      for (uint32_t ii = 0; ii < oid_len; ii++)
      {
        if (ii < ROOT_OID_LEN)
        {
          // Build up root oid on new oid
          new_oid[ii] = oid[ii];
          new_oid_len++;
          TRC_DEBUG("Building up root oid at: %u", ii);
        }
        else if (ii < ROOT_OID_LEN + 1)
        {
         // Get tag length
          new_oid[ii] = oid[ii];
          new_oid_len++;
          length_of_tag = oid[ii];;
          TRC_DEBUG("Got tag lengt of %d", length_of_tag);
        }
        else if (ii < ROOT_OID_LEN + length_of_tag + 1)
        {
          // Get tag
          new_oid[ii] = oid[ii];
          new_oid_len++;
          tag_buff[ii - ROOT_OID_LEN - 1] = oid[ii];
          TRC_DEBUG("Found letter of : %c", tag_buff[ii-ROOT_OID_LEN-1]);
        }
        else if (ii < ROOT_OID_LEN + length_of_tag + 2)
        {
          if (oid[ii] == ULONG_MAX)
          {
            tag_buff[length_of_tag - 1]++;
          }
          identifier->push_back(oid[ii]);
        }
        else {
          // Anything else is an identifier
          identifier->push_back(oid[ii]);
        }
       }
       tag_buff[length_of_tag] = '\0';
       *tag = std::string(tag_buff);
    }

    bool update_identifier(int request_type,
                           std::string tag,
                           std::vector<int>* identifier,
                           unsigned long* new_oid,
                           uint32_t &new_oid_len)
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

        if (identifier->at(0) == ULONG_MAX)
        {
          identifier->at(0) = 2;
          identifier->at(1) = 1;
          new_oid[ROOT_OID_LEN + tag.length()]++;
          return true;
        }

        if (identifier->at(0) < 2)
        // If identifier is before the start of the values, just jump to the
        // first element
        {
          identifier->at(0) = 2;
          identifier->at(1) = 1;
        }

        else
        {
        // Increment with overflow on elements. If we exceed the 5th column,
        // we've gone past the end of the table, so false
          identifier->at(1)++;
          if (identifier->at(1) > 3)
          {
            identifier->at(0)++;
            identifier->at(1) = 1;
          }
          if (identifier->at(0) > 5)
          {
            return false;
          }
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
