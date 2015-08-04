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

#include <string.h>
#include "snmp_sip_request_types.h"

namespace SNMP
{
// LCOV_EXCL_START
SIPRequestTypes string_to_request_type(char* req_string, int slen )
{
  if (!strncmp(req_string, "INVITE", slen)) { return SIPRequestTypes::INVITE; }
  else if (!strncmp(req_string, "ACK", slen)) { return SIPRequestTypes::ACK; }
  else if (!strncmp(req_string, "BYE", slen)) { return SIPRequestTypes::BYE; }
  else if (!strncmp(req_string, "CANCEL", slen)) { return SIPRequestTypes::CANCEL; }
  else if (!strncmp(req_string, "OPTIONS", slen)) { return SIPRequestTypes::OPTIONS; }
  else if (!strncmp(req_string, "REGISTER", slen)) { return SIPRequestTypes::REGISTER; }
  else if (!strncmp(req_string, "PRACK", slen)) { return SIPRequestTypes::PRACK; }
  else if (!strncmp(req_string, "SUBSCRIBE", slen)) { return SIPRequestTypes::SUBSCRIBE; }
  else if (!strncmp(req_string, "NOTIFY", slen)) { return SIPRequestTypes::NOTIFY; }
  else if (!strncmp(req_string, "PUBLISH", slen)) { return SIPRequestTypes::PUBLISH; }
  else if (!strncmp(req_string, "INFO", slen)) { return SIPRequestTypes::INFO; }
  else if (!strncmp(req_string, "REFER", slen)) { return SIPRequestTypes::REFER; }
  else if (!strncmp(req_string, "MESSAGE", slen)) { return SIPRequestTypes::MESSAGE; }
  else if (!strncmp(req_string, "UPDATE", slen)) { return SIPRequestTypes::UPDATE; }
  else { return SIPRequestTypes::OTHER; }
}
// LCOV_EXCL_STOP 
}
