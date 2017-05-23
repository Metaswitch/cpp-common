/**
 * @file snmp_scope_table.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "snmp_table.h"
#include <vector>
#include <map>
#include <string>
#include <atomic>
#include "logger.h"

#include "snmp_types.h"

#ifndef SNMP_SCOPE_TABLE_H
#define SNMP_SCOPE_TABLE_H

// This file contains the base infrastructure for SNMP tables which are indexed by scope (node
// type). It contains only abstract classes, which need to be subclassed - e.g.
// SNMP::ScalarByScopeRow


namespace SNMP
{

template <class T> class ScopeBasedRow : public Row
{
public:

  class View
  {
  public:
    View(T* data): _data(data) {};
    virtual ~View() {};
    T* get_data() { return (this->_data); }
  protected:
    T* _data;
  };

  ScopeBasedRow(std::string scope_index, View* view) :
    Row(),
    _scope_index(scope_index),
    _view(view)
  {
    // Scope based rows are indexed off a single string representing the node type.
    netsnmp_tdata_row_add_index(_row,
                                ASN_OCTET_STR,
                                _scope_index.c_str(),
                                _scope_index.length());

  };

  virtual ~ScopeBasedRow()
  {
    delete(_view);
  };

  virtual ColumnData get_columns() = 0;

protected:
  std::string _scope_index;
  View* _view;
};
}

#endif
