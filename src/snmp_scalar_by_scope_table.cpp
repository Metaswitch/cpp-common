/**
 * @file snmp_scalar_by_scope_table.cpp
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
