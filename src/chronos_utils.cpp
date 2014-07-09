/**
 * @file chronos_utils.cpp Utilities for working with chronos
 *
 * Project Clearwater - IMS in the cloud.
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

#include "chronos_utils.h"

namespace ChronosUtils
{
  HttpStackSasLogger HTTP_STACK_SAS_LOGGER;

  void HttpStackSasLogger::sas_log_rx_http_req(SAS::TrailId trail,
                                               HttpStack::Request& req,
                                               uint32_t instance_id)
  {
    log_correlator(trail, req, instance_id);
    log_req_event(trail, req, instance_id, SASEvent::HttpLogLevel::DETAIL);
  }

  void HttpStackSasLogger::sas_log_tx_http_rsp(SAS::TrailId trail,
                                               HttpStack::Request& req,
                                               int rc,
                                               uint32_t instance_id)
  {
    log_rsp_event(trail, req, rc, instance_id, SASEvent::HttpLogLevel::DETAIL);
  }

  void HttpStackSasLogger::sas_log_overload(SAS::TrailId trail,
                                            HttpStack::Request& req,
                                            int rc,
                                            uint32_t instance_id)
  {
    log_overload_event(trail,
                       req,
                       rc,
                       instance_id,
                       SASEvent::HttpLogLevel::DETAIL);
  }

} // namespace ChronosUtils

