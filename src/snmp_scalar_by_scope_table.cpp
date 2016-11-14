/**
 * @file snmp_scalar_by_scope_table.cpp
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
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include "snmp_scalar_by_scope_table.h"
#include "snmp_statistics_structures.h"
#include "snmp_internal/snmp_includes.h"
#include "snmp_internal/snmp_scope_table.h"
#include "logger.h"

namespace SNMP
{

// A ScopeBasedRow that maps the data from Scalar into the right column.
class ScalarByScopeRow: public ScopeBasedRow<Scalar>
{
public:
  ScalarByScopeRow(std::string scope_index, View* view):
    ScopeBasedRow<Scalar>(scope_index, view) {};
  ColumnData get_columns()
  {
    Scalar scalar = *(this->_view->get_data());

    // Construct and return a ColumnData with the appropriate values
    ColumnData ret;
    ret[2] = Value::uint(scalar.value);
    return ret;
  }
};

class ScalarByScopeTableImpl: public ManagedTable<ScalarByScopeRow, std::string>, public ScalarByScopeTable
{
public:
  ScalarByScopeTableImpl(std::string name,
                         std::string tbl_oid):
    ManagedTable<ScalarByScopeRow, std::string>(name,
                                                tbl_oid,
                                                2,
                                                2, // Only column 2 should be visible
                                                { ASN_OCTET_STR }) // Type of the index column
  {
    // We have a fixed number of rows, so create them in the constructor.
    scalar.value = 0;
    add("node");
  }

  void set_value(unsigned long value)
  {
    scalar.value = value;
  }

private:
  // Map row indexes to the view of the underlying data they should expose
  ScalarByScopeRow* new_row(std::string scope_index)
  {
    ScalarByScopeRow::View* view = new ScalarByScopeRow::View(&scalar);
    return new ScalarByScopeRow(scope_index, view);
  }

  Scalar scalar;
};

ScalarByScopeTable* ScalarByScopeTable::create(std::string name, std::string oid)
{
  return new ScalarByScopeTableImpl(name, oid);
}

}
