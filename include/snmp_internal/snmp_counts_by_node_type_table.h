/**
 * @file snmp_counts_by_node_type_table.h
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

#ifndef SNMP_COUNTS_BY_NODE_TYPE_TABLE_H
#define SNMP_COUNTS_BY_NODE_TYPE_TABLE_H

#include "snmp_internal/snmp_time_period_and_node_type_table.h"

namespace SNMP
{

template <class T, int i> class CountsByNodeTypeTableImpl: public ManagedTable<T, std::pair<int, int>>
{
public:
  CountsByNodeTypeTableImpl(std::string name,
                            std::string tbl_oid):
    ManagedTable<T, std::pair<int, int>>(name,
                                         tbl_oid,
                                         3,
                                         3 + i,
                                         { ASN_INTEGER , ASN_INTEGER }), // Types of the index columns
    five_second_i(5),
    five_minute_i(300),
    five_second_s(5),
    five_minute_s(300),
    five_second_b(5),
    five_minute_b(300)
  {
    this->add( std::pair<int, int>(TimePeriodIndexes::scopePrevious5SecondPeriod, NodeTypes::SCSCF) );
    this->add( std::pair<int, int>(TimePeriodIndexes::scopeCurrent5MinutePeriod, NodeTypes::SCSCF) );
    this->add( std::pair<int, int>(TimePeriodIndexes::scopePrevious5MinutePeriod, NodeTypes::SCSCF) );
    this->add( std::pair<int, int>(TimePeriodIndexes::scopePrevious5SecondPeriod, NodeTypes::ICSCF) );
    this->add( std::pair<int, int>(TimePeriodIndexes::scopeCurrent5MinutePeriod, NodeTypes::ICSCF) );
    this->add( std::pair<int, int>(TimePeriodIndexes::scopePrevious5MinutePeriod, NodeTypes::ICSCF) );
    this->add( std::pair<int, int>(TimePeriodIndexes::scopePrevious5SecondPeriod, NodeTypes::BGCF) );
    this->add( std::pair<int, int>(TimePeriodIndexes::scopeCurrent5MinutePeriod, NodeTypes::BGCF) );
    this->add( std::pair<int, int>(TimePeriodIndexes::scopePrevious5MinutePeriod, NodeTypes::BGCF) );
  }

protected:
  T* new_row(struct std::pair<int, int> indexes)
  {
    typename T::View* view = NULL;
    switch (indexes.first)
    {
      case TimePeriodIndexes::scopePrevious5SecondPeriod:
        switch (indexes.second)
        {
          case NodeTypes::ICSCF:
            view = new typename T::PreviousView(&five_second_i);
            break;
          case NodeTypes::SCSCF:
            view = new typename T::PreviousView(&five_second_s);
            break;
          case NodeTypes::BGCF:
            view = new typename T::PreviousView(&five_second_b);
            break;
        }
        break;
      case TimePeriodIndexes::scopeCurrent5MinutePeriod:
        switch (indexes.second)
        {
          case NodeTypes::ICSCF:
            view = new typename T::CurrentView(&five_minute_i);
            break;
          case NodeTypes::SCSCF:
            view = new typename T::CurrentView(&five_minute_s);
            break;
          case NodeTypes::BGCF:
            view = new typename T::CurrentView(&five_minute_b);
            break;
        }
        break;
      case TimePeriodIndexes::scopePrevious5MinutePeriod:
        switch (indexes.second)
        {
          case NodeTypes::ICSCF:
            view = new typename T::PreviousView(&five_minute_i);
            break;
          case NodeTypes::SCSCF:
            view = new typename T::PreviousView(&five_minute_s);
            break;
          case NodeTypes::BGCF:
            view = new typename T::PreviousView(&five_minute_b);
            break;
        }
        break;
    }
    return new T(indexes.first, indexes.second, view);
  }

  typename T::CurrentAndPrevious five_second_i;
  typename T::CurrentAndPrevious five_minute_i;
  typename T::CurrentAndPrevious five_second_s;
  typename T::CurrentAndPrevious five_minute_s;
  typename T::CurrentAndPrevious five_second_b;
  typename T::CurrentAndPrevious five_minute_b;
};

template <class T, int i> class CountsByNodeTypeTable
{
public:
  virtual ~CountsByNodeTypeTable() {};

  CountsByNodeTypeTable* create(std::string name,
                                std::string tbl_oid)
  {
    return new CountsByNodeTypeTableImpl<T, i>(name, tbl_oid);
  }

  CountsByNodeTypeTable() {};
};

}

#endif
