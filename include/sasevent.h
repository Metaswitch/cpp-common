/**
 * @file sasevent.h
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
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
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#ifndef SASEVENT_H__
#define SASEVENT_H__

#include <string>

namespace SASEvent {

  const std::string CURRENT_RESOURCE_BUNDLE = "org.projectclearwater.20140723";

  // Name of the HTTP header we use to correlate the client and server in SAS.
  const std::string HTTP_BRANCH_HEADER_NAME = "X-SAS-HTTP-Branch-ID";

  // The levels at which clearwater nodes may log HTTP messages.
  enum struct HttpLogLevel
  {
    DETAIL = 40,
    PROTOCOL = 60
  };

  //----------------------------------------------------------------------------
  // Event spaces.
  //----------------------------------------------------------------------------
  const int COMMON_BASE = 0x000000;
  const int SPROUT_BASE = 0x810000;
  const int HOMESTEAD_BASE = 0x820000;
  const int RALF_BASE = 0x830000;
  const int MEMENTO_BASE = 0x840000;
  const int GEMINI_BASE = 0x850000;

  //----------------------------------------------------------------------------
  // Common events and protocol flows.
  //----------------------------------------------------------------------------
  const int RX_SIP_MSG = COMMON_BASE + 0x000001;
  const int TX_SIP_MSG = COMMON_BASE + 0x000002;

  const int TX_HTTP_REQ = COMMON_BASE + 0x000003;
  const int RX_HTTP_REQ = COMMON_BASE + 0x000004;
  const int TX_HTTP_RSP = COMMON_BASE + 0x000005;
  const int RX_HTTP_RSP = COMMON_BASE + 0x000006;
  const int HTTP_REQ_ERROR = COMMON_BASE + 0x000007;
  const int HTTP_REJECTED_OVERLOAD = COMMON_BASE + 0x000008;

  // Duplicates of the above HTTP events, but logged at detail level (40)
  // rather than protocol level (60).
  const int TX_HTTP_REQ_DETAIL = COMMON_BASE + 0x000009;
  const int RX_HTTP_REQ_DETAIL = COMMON_BASE + 0x00000A;
  const int TX_HTTP_RSP_DETAIL = COMMON_BASE + 0x00000B;
  const int RX_HTTP_RSP_DETAIL = COMMON_BASE + 0x00000C;
  const int HTTP_REQ_ERROR_DETAIL = COMMON_BASE + 0x00000D;
  const int HTTP_REJECTED_OVERLOAD_DETAIL = COMMON_BASE + 0x00000E;

  const int DIAMETER_TX = COMMON_BASE + 0x00000F;
  const int DIAMETER_RX = COMMON_BASE + 0x000010;
  const int DIAMETER_TIMEOUT = COMMON_BASE + 0x000011;

  const int MEMCACHED_GET_START = COMMON_BASE + 0x000100;
  const int MEMCACHED_GET_SUCCESS = COMMON_BASE + 0x000101;
  const int MEMCACHED_GET_NOT_FOUND = COMMON_BASE + 0x000102;
  const int MEMCACHED_GET_ERROR = COMMON_BASE + 0x000103;

  const int MEMCACHED_SET_START = COMMON_BASE + 0x000104;
  const int MEMCACHED_SET_CONTENTION = COMMON_BASE + 0x000105;
  const int MEMCACHED_SET_FAILED = COMMON_BASE + 0x000106;

  const int MEMCACHED_DELETE = COMMON_BASE + 0x000107;

  const int BASERESOLVE_SRV_RESULT = COMMON_BASE + 0x000200;
  const int BASERESOLVE_A_RESULT = COMMON_BASE + 0x000201;

  const int CASS_CONNECT_FAIL = COMMON_BASE + 0x0300;

} // namespace SASEvent

#endif
