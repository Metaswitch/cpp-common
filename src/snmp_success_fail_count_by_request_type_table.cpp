/**
 * @file snmp_success_fail_count_by_request_type_table.cpp
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

#include "snmp_success_fail_count_by_request_type_table.h"
#include "snmp_internal/snmp_includes.h"
#include "snmp_internal/snmp_counts_by_other_type_table.h"
#include "snmp_statistics_structures.h"
#include "snmp_sip_request_types.h"
#include "logger.h"

namespace SNMP
{

// Time and Node Based Row that maps the data from SuccessFailCount into the right column.
class SuccessFailCountByRequestTypeRow: public TimeAndOtherTypeBasedRow<SuccessFailCount>
{
public:
  SuccessFailCountByRequestTypeRow(int time_index, int type_index, View* view):
    TimeAndOtherTypeBasedRow<SuccessFailCount>(time_index, type_index, view) {};
  ColumnData get_columns()
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    SuccessFailCount* counts = _view->get_data(now);
    uint_fast32_t attempts = counts->attempts.load();
    uint_fast32_t successes = counts->successes.load();
    uint_fast32_t failures = counts->failures.load();
    uint_fast32_t success_percent_in_ten_thousands = 0;
    if (attempts == uint_fast32_t(0))
    {
      // If there are no attempts made we report the Success Percent as being
      // 100% to indicate that there have been no errors.
      // Note that units for Success Percent are actually 10,000's of a percent.
      success_percent_in_ten_thousands = 100 * 10000;
    }
    else if (successes > 0)
    {
      // Units for Success Percent are actually 10,000's of a percent.
      success_percent_in_ten_thousands = (successes * 100 * 10000) / (successes + failures);
    }

    // Construct and return a ColumnData with the appropriate values
    ColumnData ret;
    ret[1] = Value::integer(this->_index);
    ret[2] = Value::integer(this->_type_index);
    ret[3] = Value::uint(attempts);
    ret[4] = Value::uint(successes);
    ret[5] = Value::uint(failures);
    ret[6] = Value::uint(success_percent_in_ten_thousands);
    return ret;
  }
  static int get_count_size() { return 4; }
};

static std::vector<int> request_types =
{
  SIPRequestTypes::INVITE,
  SIPRequestTypes::ACK,
  SIPRequestTypes::BYE,
  SIPRequestTypes::CANCEL,
  SIPRequestTypes::OPTIONS,
  SIPRequestTypes::REGISTER,
  SIPRequestTypes::PRACK,
  SIPRequestTypes::SUBSCRIBE,
  SIPRequestTypes::NOTIFY,
  SIPRequestTypes::PUBLISH,
  SIPRequestTypes::INFO,
  SIPRequestTypes::REFER,
  SIPRequestTypes::MESSAGE,
  SIPRequestTypes::UPDATE,
  SIPRequestTypes::OTHER
};

class SuccessFailCountByRequestTypeTableImpl: public CountsByOtherTypeTableImpl<SuccessFailCountByRequestTypeRow, SuccessFailCount>,
  public SuccessFailCountByRequestTypeTable
{
public:
  SuccessFailCountByRequestTypeTableImpl(std::string name,
                                         std::string tbl_oid):
    CountsByOtherTypeTableImpl<SuccessFailCountByRequestTypeRow,
                               SuccessFailCount>(name,
                                                 tbl_oid,
                                                 request_types)
  {}

  void increment_attempts(SIPRequestTypes type)
  {
    five_second[type]->get_current()->attempts++;
    five_minute[type]->get_current()->attempts++;
  }

  void increment_successes(SIPRequestTypes type)
  {
    five_second[type]->get_current()->successes++;
    five_minute[type]->get_current()->successes++;
  }

  void increment_failures(SIPRequestTypes type)
  {
    five_second[type]->get_current()->failures++;
    five_minute[type]->get_current()->failures++;
  }
};

SuccessFailCountByRequestTypeTable* SuccessFailCountByRequestTypeTable::create(std::string name,
                                                                               std::string oid)
{
  return new SuccessFailCountByRequestTypeTableImpl(name, oid);
}

}
