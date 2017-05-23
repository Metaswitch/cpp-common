/**
 * @file snmp_row.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <vector>
#include <map>
#include <string>

#include "snmp_internal/snmp_includes.h"
#include "snmp_row.h"
#include "log.h"

namespace SNMP
{
Value Value::uint(uint32_t val)
{
  return Value(ASN_UNSIGNED, (unsigned char*)&val, sizeof(uint32_t));
};

// Utility constructor for ASN_INTEGERS
Value Value::integer(int val)
{
  return Value(ASN_INTEGER, (unsigned char*)&val, sizeof(int32_t));
};


Row::Row()
{
  _row = netsnmp_tdata_create_row();
  _row->data = this;
}

Row::~Row()
{
  netsnmp_tdata_delete_row(_row);
}
}
