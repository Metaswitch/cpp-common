/**
 * @file snmp_scalar.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>

#include "snmp_internal/snmp_includes.h"
#include "snmp_scalar.h"

namespace SNMP
{
U32Scalar::U32Scalar(std::string name,
                     std::string oid_str):
  value(0),
  _registered_oid(oid_str + ".0")
  {
    oid parsed_oid[64];
    size_t oid_len = 64;
    read_objid(_registered_oid.c_str(), parsed_oid, &oid_len);
    netsnmp_register_read_only_ulong_instance(name.c_str(),
                                              parsed_oid,
                                              oid_len,
                                              &value,
                                              NULL);
  }

U32Scalar::~U32Scalar()
{
  oid parsed_oid[64];
  size_t oid_len = 64;
  read_objid(_registered_oid.c_str(), parsed_oid, &oid_len);
  // Call into netsnmp to unregister this OID.
  unregister_mib(parsed_oid, oid_len);
}

void U32Scalar::set_value(unsigned long val)
{
  value = val;
}
}
