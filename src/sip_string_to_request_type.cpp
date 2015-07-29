/**
 * @file sip_string_to_request_type.cpp
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

extern "C" {
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
}
#include "snmp_sip_request_types.h"

namespace SNMP
{
// LCOV_EXCL_START
SIPRequestTypes string_to_request_type(const pj_str_t* req_string)
{
  if (!pj_stricmp2(req_string, "INVITE")) { return SIPRequestTypes::INVITE; }
  else if (!pj_stricmp2(req_string, "ACK")) { return SIPRequestTypes::ACK; }
  else if (!pj_stricmp2(req_string, "BYE")) { return SIPRequestTypes::BYE; }
  else if (!pj_stricmp2(req_string, "CANCEL")) { return SIPRequestTypes::CANCEL; }
  else if (!pj_stricmp2(req_string, "OPTIONS")) { return SIPRequestTypes::OPTIONS; }
  else if (!pj_stricmp2(req_string, "REGISTER")) { return SIPRequestTypes::REGISTER; }
  else if (!pj_stricmp2(req_string, "PRACK")) { return SIPRequestTypes::PRACK; }
  else if (!pj_stricmp2(req_string, "SUBSCRIBE")) { return SIPRequestTypes::SUBSCRIBE; }
  else if (!pj_stricmp2(req_string, "NOTIFY")) { return SIPRequestTypes::NOTIFY; }
  else if (!pj_stricmp2(req_string, "PUBLISH")) { return SIPRequestTypes::PUBLISH; }
  else if (!pj_stricmp2(req_string, "INFO")) { return SIPRequestTypes::INFO; }
  else if (!pj_stricmp2(req_string, "REFER")) { return SIPRequestTypes::REFER; }
  else if (!pj_stricmp2(req_string, "MESSAGE")) { return SIPRequestTypes::MESSAGE; }
  else if (!pj_stricmp2(req_string, "UPDATE")) { return SIPRequestTypes::UPDATE; }
  else { return SIPRequestTypes::OTHER; }
}
// LCOV_EXCL_STOP 
}
