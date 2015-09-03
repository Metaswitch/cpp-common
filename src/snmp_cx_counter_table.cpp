/**
 * @file snmp_single_cx_counter_table.cpp
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

#include "snmp_internal/snmp_includes.h"
#include "logger.h"

namespace SNMP
{

enum AppId
{
  BASE = 0,
  3GPP = 1,
  TIMEOUT = 2
}

static std::vector<int> base_result_codes = 
{
  1001,
  2001, 2002,
  3000, 3001, 3002, 3003, 30004, 3005, 3006, 3007, 3008, 3009, 3010,
  4001, 4002, 4003,
  5001, 5002, 5003, 5004, 5005, 5006, 5007, 5008, 5009, 5010, 5011, 5012, 5013,
  5014, 5015, 5016, 5017
}

static std::vector<int> 3gpp_result_codes = 
{
  2001, 2002,
  5001, 5002, 5003, 5004, 5005, 5006, 5007, 5008, 5009, 5011 
}


// Storage for the underlying data
struct SingleCount
{
  std::atomic_uint_fast64_t count;
  void reset(uint64_t time_periodstart, SingleCount* previous = NULL) { count = 0; };
};

// Time and Double Indexed Row that maps the data from SingleCount into the right column.
class CxCounterRow: public TimeAndDoubleIndexedRow<SingleCount>
{
public:
  CxCounterRow(int time_index, AppID app_id, ResultCode result_code View* view):
    TimeAndDoubleIndexedRow<SingleCount>(time_index, app_id, result_code, view) {};

  ColumnData get_columns()
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    SingleCount* count = *(this->_view->get_data(now));

    // Construct and return a ColumnData with the appropriate values
    ColumnData ret;
    ret[1] = Value::integer(this->_index);
    ret[2] = Value::integer(this->_first_index);
    ret[3] = Value::integer(this->_second_index);
    ret[4] = Value::uint(count->count.load());
    return ret;
  }

};

class CxCounterTableImpl: public CountsByDoubleIndexedTableImpl<CxCounterRow>, public CxCounterTable
{
public:
  CxCounterTableImpl(std::string name,
                     std::string tbl_oid):
    CountsByDoubleIndexedTableImpl<CxCounterRow>(name,
                                                 tbl_oid,
                                                 base_result_codes,
                                                 3gpp_result_codes):
      ManagedTable<CxCounterRow, int>(name,
                                      tbl_oid,
                                      4,
                                      4,
                                      { ASN_INTEGER , ASN_INTEGER , ASN_INTEGER })
  {
    int n = 0;

    for (std::vector<int>::iterator code = base_result_codes.begin();
         code != base_result_codes.end();
         code++)
    {
      five_second[AppId::BASE][*code] = new typename CxCounterRow::currentAndPrevious(5000);
      five_second[AppId::BASE][*code] = new typename CxCounterRow::currentAndPrevious(300000);

      this->add(n++, new CxCounterRow(TimePeriodIndexes::scopePrevious5SecondPeriod,
                                      AppId::BASE,
                                      *code,
                                      new CxCounterRow::PreviousView(five_second[AppId::BASE][*code])));
      this->add(n++, new CxCounterRow(TimePeriodIndexes::scopeCurrent5MinutePeriod,
                                      AppId::BASE,
                                      *code,
                                      new CxCounterRow::CurrentView(five_minute[AppId::BASE][*code])));
      this->add(n++, new CxCounterRow(TimePeriodIndexes::scopePrevious5MinutePeriod,
                                      AppId::BASE,
                                      *code,
                                      new CxCounterRow::PreviousView(five_minute[AppId::BASE][*code])));
   }

    for (std::vector<int>::iterator code = 3gpp_result_codes.begin();
         code != 3gpp_result_codes.end();
         code++)
    {
      five_second[AppId::3GPP][*code] = new typename CxCounterRow::currentAndPrevious(5000);
      five_second[AppId::3GPP][*code] = new typename CxCounterRow::currentAndPrevious(300000);

      this->add(n++, new CxCounterRow(TimePeriodIndexes::scopePrevious5SecondPeriod,
                                      AppId::3GPP,
                                      *code,
                                      new CxCounterRow::PreviousView(five_second[AppId::3GPP][*code])));
      this->add(n++, new CxCounterRow(TimePeriodIndexes::scopeCurrent5MinutePeriod,
                                      AppId::3GPP,
                                      *code,
                                      new CxCounterRow::CurrentView(five_minute[AppId::3GPP][*code])));
      this->add(n++, new CxCounterRow(TimePeriodIndexes::scopePrevious5MinutePeriod,
                                      AppId::3GPP,
                                      *code,
                                      new CxCounterRow::PreviousView(five_minute[AppId::3GPP][*code])));
    }
  
  void increment(AppId app_id, ResultCode result_code)
  {
    five_second[app_id][result_code]->get_current()->count++;
    five_minute[app_id][result_code]->get_current()->count++;
  }
  
  }
};

CxCounterTable* CxCounterTable::create(std::string name,
                                       std::string oid)
{
  return new CxCounterTableImpl(name, oid);
}

}
