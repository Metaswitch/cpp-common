/**
 * @file snmp_time_period_and_scope_table.h
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2016 Metaswitch Networks Ltd
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
 * software and licensed und er the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include "snmp_time_period_table.h"
#include "snmp_node_types.h"

#ifndef SNMP_TIME_PERIOD_AND_SCOPE_TABLE_H
#define SNMP_TIME_PERIOD_AND_SCOPE_TABLE_H

// This file contains the base infrastructure for SNMP tables
// which are indexed by time period and scope (node type).
namespace SNMP
{

template <class T> class TimeAndScopeBasedRow : public TimeBasedRow<T>
{
public:
  // Constructor, takes ownership of the View*.
  TimeAndScopeBasedRow(int time_index, std::string scope_index, typename TimeBasedRow<T>::View* view) :
    TimeBasedRow<T>(time_index, view),
    _scope_index(scope_index)
  {
    // Add the scope index (the time index is added in the base class)
    netsnmp_tdata_row_add_index(this->_row,
                                ASN_OCTET_STR,
                                _scope_index.c_str(),
                                _scope_index.length());
  };

  virtual ~TimeAndScopeBasedRow()
  {
  };

protected:
  std::string _scope_index;
};

}

#endif
