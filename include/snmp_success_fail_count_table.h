/* @file snmp_success_fail_count_table.h
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <vector>
#include <map>
#include <string>

#ifndef SNMP_SUCCESS_FAIL_COUNT_TABLE_H
#define SNMP_SUCCESS_FAIL_COUNT_TABLE_H

// This file contains the interface for tables which:
//   - are indexed by time period
//   - increment a count of the attempts, successes and failures over time
//   - report a count of the attempts, successes and failures and the
//     percentage of attempts that were successful.
//
// This is defined as an interface in order not to pollute the codebase with netsnmp include files
// (which indiscriminately #define things like READ and WRITE).

namespace SNMP
{
class SuccessFailCountTable
{
public:
  static SuccessFailCountTable* create(std::string name, std::string oid);
  virtual void increment_attempts() = 0;
  virtual void increment_successes() = 0;
  virtual void increment_failures() = 0;
  virtual ~SuccessFailCountTable() {};

protected:
  SuccessFailCountTable() {};
};

struct RegistrationStatsTables
{
  SuccessFailCountTable* init_reg_tbl;
  SuccessFailCountTable* re_reg_tbl;
  SuccessFailCountTable* de_reg_tbl;
};

struct AuthenticationStatsTables
{
  SuccessFailCountTable* sip_digest_auth_tbl;
  SuccessFailCountTable* ims_aka_auth_tbl;
  SuccessFailCountTable* non_register_auth_tbl;
};

}
#endif
