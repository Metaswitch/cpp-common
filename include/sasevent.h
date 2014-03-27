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

  const std::string CURRENT_RESOURCE_BUNDLE = "org.projectclearwater.20140326";

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

  const int DIAMETER_TX_REQ = COMMON_BASE + 0x00000F;
  const int DIAMETER_RX_REQ = COMMON_BASE + 0x000010;
  const int DIAMETER_TX_RSP = COMMON_BASE + 0x000011;
  const int DIAMETER_RX_RSP = COMMON_BASE + 0x000012;
  const int DIAMETER_REQ_TIMEOUT = COMMON_BASE + 0x000013;

  //----------------------------------------------------------------------------
  // Sprout events.
  //----------------------------------------------------------------------------
  const int ENUM_START = SPROUT_BASE + 0x000000;
  const int ENUM_MATCH = SPROUT_BASE + 0x000001;
  const int ENUM_INCOMPLETE = SPROUT_BASE + 0x000002;
  const int ENUM_COMPLETE = SPROUT_BASE + 0x000003;
  const int TX_ENUM_REQ = SPROUT_BASE + 0x000004;
  const int RX_ENUM_RSP = SPROUT_BASE + 0x000005;
  const int RX_ENUM_ERR = SPROUT_BASE + 0x000006;

  // Homestead events

  const int INVALID_SCHEME = HOMESTEAD_BASE + 0x0000;
  const int NO_IMPU_AKA = HOMESTEAD_BASE + 0x0010;
  const int NO_AV_CACHE = HOMESTEAD_BASE + 0x0020;
  const int NO_AV_HSS = HOMESTEAD_BASE + 0x0030;
  const int INVALID_REG_TYPE = HOMESTEAD_BASE + 0x0040;
  const int SUB_NOT_REG = HOMESTEAD_BASE + 0x0050;
  const int NO_SUB_CACHE = HOMESTEAD_BASE + 0x0060;
  const int NO_REG_DATA_CACHE = HOMESTEAD_BASE + 0x0070;
  const int REG_DATA_HSS_SUCCESS = HOMESTEAD_BASE + 0x0080;
  const int REG_DATA_HSS_FAIL = HOMESTEAD_BASE + 0x0090;
  const int ICSCF_NO_HSS = HOMESTEAD_BASE + 0x00A0;
  const int REG_STATUS_HSS_FAIL = HOMESTEAD_BASE + 0x00B0;
  const int LOC_INFO_HSS_FAIL = HOMESTEAD_BASE + 0x00C0;
  const int INVALID_DEREG_REASON = HOMESTEAD_BASE + 0x00D0;
  const int NO_IMPU_DEREG = HOMESTEAD_BASE + 0x00E0;
  const int DEREG_FAIL = HOMESTEAD_BASE + 0x00F0;
  const int DEREG_SUCCESS = HOMESTEAD_BASE + 0x0100;
  const int UPDATED_IMS_SUBS = HOMESTEAD_BASE + 0x0110;
  const int CASS_CONNECT_FAIL = HOMESTEAD_BASE + 0x0120;
  const int CACHE_GET_AV = HOMESTEAD_BASE + 0x013;
  const int CACHE_GET_AV_SUCCESS = HOMESTEAD_BASE + 0x0140;
  const int CACHE_PUT_ASSOC_IMPU = HOMESTEAD_BASE + 0x0150;
  const int CACHE_GET_ASSOC_IMPU = HOMESTEAD_BASE + 0x0160;
  const int CACHE_GET_ASSOC_IMPU_SUCCESS = HOMESTEAD_BASE + 0x0170;
  const int CACHE_GET_ASSOC_IMPU_FAIL = HOMESTEAD_BASE + 0x0180;
  const int CACHE_GET_IMS_SUB = HOMESTEAD_BASE + 0x0190;
  const int CACHE_GET_IMS_SUB_SUCCESS = HOMESTEAD_BASE + 0x01A0;
  const int CACHE_ASSOC_IMPI = HOMESTEAD_BASE + 0x01B0;
  const int CACHE_PUT_IMS_SUB = HOMESTEAD_BASE + 0x01C0;
  const int CACHE_DELETE_IMPUS = HOMESTEAD_BASE + 0x01D0;
  const int CACHE_GET_ASSOC_PRIMARY_IMPUS = HOMESTEAD_BASE + 0x01E0;
  const int CACHE_GET_ASSOC_PRIMARY_IMPUS_SUCCESS = HOMESTEAD_BASE + 0x01F0;
  const int CACHE_DISASSOC_REG_SET = HOMESTEAD_BASE + 0x0200;
  const int CACHE_DELETE_IMPI_MAP = HOMESTEAD_BASE + 0x0210;

  // Ralf events

  const int NEW_RF_SESSION = RALF_BASE + 0x000;
  const int CONTINUED_RF_SESSION = RALF_BASE + 0x100;
  const int END_RF_SESSION = RALF_BASE + 0x200;

  const int BILLING_REQUEST_SENT = RALF_BASE + 0x300;

  const int INTERIM_TIMER_POPPED = RALF_BASE + 0x400;
  const int INTERIM_TIMER_CREATED = RALF_BASE + 0x500;
  const int INTERIM_TIMER_RENEWED = RALF_BASE + 0x600;

  const int INCOMING_REQUEST = RALF_BASE + 0x700;
  const int REQUEST_REJECTED = RALF_BASE + 0x800;

  const int CDF_FAILOVER = RALF_BASE + 0x900;
  const int BILLING_REQUEST_NOT_SENT = RALF_BASE + 0xA00;
  const int BILLING_REQUEST_REJECTED = RALF_BASE + 0xB00;
  const int BILLING_REQUEST_SUCCEEDED = RALF_BASE + 0xC00;


} // namespace SASEvent

#endif
