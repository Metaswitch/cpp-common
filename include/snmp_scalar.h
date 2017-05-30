/**
 * @file snmp_scalar.h
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>
#include "snmp_abstract_scalar.h"

#ifndef SNMP_SCALAR_H
#define SNMP_SCALAR_H

// This file contains infrastructure for SNMP scalars (single values, not in a
// table).
//
// To use one, simply create a U32Scalar and modify its `value` object as
// necessary - changes to this will automatically be reflected over SNMP. For
// example:
//
//     SNMP::U32Scalar* cxn_count = new SNMP::U32Scalar("bono_cxn_count", ".1.2.3");
//     cxn_count->value = 42;
//
// Note that the OID scalars are exposed under has an additional element with
// the value zero (so using the example above, would actually be obtained by
// querying ".1.2.3.0"). This is extremely counter-intuitive and easy to
// forget. Because of this the trailing ".0" should not be specified when
// constructing the scalar - the scalar will add it when registering with
// net-snmp.

namespace SNMP
{

// Exposes a number as an SNMP Unsigned32.
class U32Scalar: public AbstractScalar
{
public:
  /// Constructor
  ///
  /// @param name - The name of the scalar.
  /// @param oid  - The OID for the scalar excluding the trailing ".0"
  U32Scalar(std::string name, std::string oid);
  ~U32Scalar();
  virtual void set_value(unsigned long val);
  unsigned long value;

private:
  // The OID as registered with net-snmp (including the trailing ".0").
  std::string _registered_oid;
};

}
#endif
