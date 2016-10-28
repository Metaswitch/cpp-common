/* @file snmp_success_fail_count_table.h
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
 * software and licensed und er the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
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
