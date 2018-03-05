/**
 * @file snmp_single_count_by_node_type_table.cpp
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "snmp_single_count_by_node_type_table.h"
#include "snmp_statistics_structures.h"
#include "snmp_internal/snmp_includes.h"
#include "snmp_internal/snmp_counts_by_other_type_table.h"
#include "snmp_node_types.h"
#include "logger.h"

namespace SNMP
{

// Time and Node Based Row that maps the data from SingleCount into the right column.
class SingleCountByNodeTypeRow: public TimeAndOtherTypeBasedRow<SingleCount>
{
public:
  SingleCountByNodeTypeRow(int time_index, int type_index, View* view):
    TimeAndOtherTypeBasedRow<SingleCount>(time_index, type_index, view) {};
  ColumnData get_columns()
  {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);

    SingleCount accumulated = *(this->_view->get_data(now));

    // Construct and return a ColumnData with the appropriate values
    ColumnData ret;
    ret[1] = Value::integer(this->_index);
    ret[2] = Value::integer(this->_type_index);
    ret[3] = Value::uint(accumulated.count);
    return ret;
  }

  static int get_count_size() { return 1; }
};

class SingleCountByNodeTypeTableImpl: public CountsByOtherTypeTableImpl<SingleCountByNodeTypeRow, SingleCount>, public SingleCountByNodeTypeTable
{
public:
  SingleCountByNodeTypeTableImpl(std::string name,
                                 std::string tbl_oid,
                                 std::vector<int> node_types): CountsByOtherTypeTableImpl<SingleCountByNodeTypeRow, SingleCount>(name, tbl_oid, node_types)
  {}

  void increment(NodeTypes type)
  {
    five_second[type]->get_current()->count++;
    five_minute[type]->get_current()->count++;
  }
};

SingleCountByNodeTypeTable* SingleCountByNodeTypeTable::create(std::string name,
                                                               std::string oid,
                                                               std::vector<int> node_types)
{
  return new SingleCountByNodeTypeTableImpl(name, oid, node_types);
}

}
