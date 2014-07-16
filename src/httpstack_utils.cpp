/**
 * @file httpstack_utils.cpp Utility classes and functions for use with the
 * HttpStack.
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

#include "httpstack_utils.h"

namespace HttpStackUtils
{
  //
  // PingController methods.
  //
  void PingController::process_request(HttpStack::Request& req,
                                       SAS::TrailId trail)
  {
    req.add_content("OK");
    req.send_reply(200, trail);
  }

  ControllerThreadPool::ControllerThreadPool(unsigned int num_threads,
                                             unsigned int max_queue) :
    ThreadPool<Handler*>(num_threads, max_queue)
  {}

  // This function defines how the worker threads process received handlers.
  // This is simply to invoke the run() method on the underlying handler.
  void ControllerThreadPool::process_work(HttpStackUtils::Handler*& handler)
  {
    handler->run();
  }

  //
  // Chronos utilities.
  //

  // Declaration of a logger for logging chronos flows.
  ChronosSasLogger CHRONOS_SAS_LOGGER;

  // Log a request from chronos.
  void ChronosSasLogger::sas_log_rx_http_req(SAS::TrailId trail,
                                             HttpStack::Request& req,
                                             uint32_t instance_id)
  {
    log_correlator(trail, req, instance_id);
    log_req_event(trail, req, instance_id, SASEvent::HttpLogLevel::DETAIL);
  }

  // Log a response to chronos.
  void ChronosSasLogger::sas_log_tx_http_rsp(SAS::TrailId trail,
                                             HttpStack::Request& req,
                                             int rc,
                                             uint32_t instance_id)
  {
    log_rsp_event(trail, req, rc, instance_id, SASEvent::HttpLogLevel::DETAIL);
  }

  // Log when a chronos request is rejected due to overload.
  void ChronosSasLogger::sas_log_overload(SAS::TrailId trail,
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

} // namespace HttpStackUtils
