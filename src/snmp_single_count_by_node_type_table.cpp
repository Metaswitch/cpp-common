/**
 * @file snmp_single_count_by_node_type_table.cpp
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

#include "snmp_single_count_by_node_type_table.h"
#include "snmp_internal/snmp_includes.h"
#include "snmp_internal/snmp_counts_by_node_type_table.h"
#include "logger.h"

namespace SNMP
{

// Storage for the underlying data
struct SingleCount
{
  uint64_t count;
  void reset() { count = 0; };
};

// Time and Node Based Row that maps the data from SingleCount into the right column.
class SingleCountByNodeTypeRow: public TimeAndNodeTypeBasedRow<SingleCount>
{
public:
  SingleCountByNodeTypeRow(int time_index, int type_index, View* view):
    TimeAndNodeTypeBasedRow<SingleCount>(time_index, type_index, view) {};
  ColumnData get_columns()
  {
    SingleCount accumulated = *(this->_view->get_data());

    // Construct and return a ColumnData with the appropriate values
    ColumnData ret;
    ret[1] = Value::integer(this->_index);
    ret[2] = Value::integer(this->_type_index);
    ret[3] = Value::uint(accumulated.count);
    return ret;
  }
};

class SingleCountByNodeTypeTableImpl: public CountsByNodeTypeTableImpl<SingleCountByNodeTypeRow, 0>, public SingleCountByNodeTypeTable
{
public:
  SingleCountByNodeTypeTableImpl(std::string name,
                                 std::string tbl_oid):
    CountsByNodeTypeTableImpl<SingleCountByNodeTypeRow, 0>(name, tbl_oid)
  {}
 
  void increment(NodeTypes type)
  {
    five_second[type]->get_current()->count++;
    five_minute[type]->get_current()->count++;
  }
};

SingleCountByNodeTypeTable* SingleCountByNodeTypeTable::create(std::string name,
                                                               std::string oid)
{
  return new SingleCountByNodeTypeTableImpl(name, oid);
}

}
