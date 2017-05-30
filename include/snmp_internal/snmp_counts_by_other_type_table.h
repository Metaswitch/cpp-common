/**
 * @file snmp_counts_by_other_type_table.h
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef SNMP_COUNTS_BY_OTHER_TYPE_TABLE_H
#define SNMP_COUNTS_BY_OTHER_TYPE_TABLE_H

#include "snmp_internal/snmp_time_period_and_other_type_table.h"

namespace SNMP
{

template <class RowType, class DataType> class CountsByOtherTypeTableImpl: public ManagedTable<RowType, int>
{
public:
  CountsByOtherTypeTableImpl(std::string name,
                             std::string tbl_oid,
                             std::vector<int> types):
    ManagedTable<RowType, int>(name,
                         tbl_oid,
                         3,
                         3 + RowType::get_count_size() - 1,
                         { ASN_INTEGER , ASN_INTEGER }) // Types of the index columns
  {
    int n = 0;

    for (std::vector<int>::iterator type = types.begin();
         type != types.end();
         type++)
    {
      five_second[*type] = new CurrentAndPrevious<DataType>(5000);
      five_minute[*type] = new CurrentAndPrevious<DataType>(300000);

      this->add(n++, new RowType(TimePeriodIndexes::scopePrevious5SecondPeriod, *type, new typename RowType::PreviousView(five_second[*type])));
      this->add(n++, new RowType(TimePeriodIndexes::scopeCurrent5MinutePeriod, *type, new typename RowType::CurrentView(five_minute[*type])));
      this->add(n++, new RowType(TimePeriodIndexes::scopePrevious5MinutePeriod, *type, new typename RowType::PreviousView(five_minute[*type])));
    }
  }

  ~CountsByOtherTypeTableImpl()
  {
    for (typename std::map<int, CurrentAndPrevious<DataType>*>::iterator type = five_second.begin();
         type != five_second.end();
         type++)
    {
      delete type->second;
    }

    for (typename std::map<int, CurrentAndPrevious<DataType>*>::iterator type = five_minute.begin();
         type != five_minute.end();
         type++)
    {
      delete type->second;
    }
  }

protected:
  RowType* new_row(int indexes) { return NULL; };

  std::map<int, CurrentAndPrevious<DataType>*> five_second;
  std::map<int, CurrentAndPrevious<DataType>*> five_minute;
};

}

#endif
