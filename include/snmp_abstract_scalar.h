/**
 * @file snmp_abstract_scalar.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>

#ifndef SNMP_ABSTRACT_SCALAR_H
#define SNMP_ABSTRACT_SCALAR_H

// This file contains a base abstract class for SNMP scalars e.g. U32Scalar,
// ScalarByScopeTable.

namespace SNMP
{

// Exposes a number as an SNMP Unsigned32.
class AbstractScalar
{
public:
  AbstractScalar() {};
  virtual ~AbstractScalar() {};
  virtual void set_value(unsigned long value) = 0;
};

}
#endif
