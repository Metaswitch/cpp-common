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
  // PingHandler methods.
  //
  void PingHandler::process_request(HttpStack::Request& req,
                                    SAS::TrailId trail)
  {
    req.add_content("OK");
    req.send_reply(200, trail);
  }

  //
  // HandlerThreadPool methods.
  //
  HandlerThreadPool::HandlerThreadPool(unsigned int num_threads,
                                       unsigned int max_queue) :
    _pool(num_threads, max_queue), _wrappers()
  {
    _pool.start();
  }

  HttpStack::HandlerInterface*
    HandlerThreadPool::wrap(HttpStack::HandlerInterface* handler)
  {
    // Create a new wrapper around the specific handler and record it in
    // the wrappers vector.
    Wrapper* wrapper = new Wrapper(&_pool, handler);
    _wrappers.push_back(wrapper);
    return wrapper;
  }

  HandlerThreadPool::~HandlerThreadPool()
  {
    // The thread pool owns all the wrappers it creates.  Delete them.
    for(std::vector<Wrapper*>::iterator it = _wrappers.begin();
        it != _wrappers.end();
        ++it)
    {
      delete *it;
    }

    _wrappers.clear();

    // Terminate the thread pool.
    _pool.stop();
    _pool.join();
  }

  HandlerThreadPool::Pool::Pool(unsigned int num_threads,
                                unsigned int max_queue) :
    ThreadPool<RequestParams*>(num_threads, max_queue)
  {}

  // This function defines how the worker threads process received requests.
  // This is simply to invoke the process_request() method on the underlying
  // handler.
  void HandlerThreadPool::Pool::
    process_work(HttpStackUtils::HandlerThreadPool::RequestParams*& params)
  {
    params->handler->process_request(params->request, params->trail);
    delete params; params = NULL;
  }

  HandlerThreadPool::Wrapper::Wrapper(Pool* pool,
                                      HandlerInterface* handler) :
    _pool(pool), _handler(handler)
  {}

  // Implementation of HandlerInterface::process_request().  This builds a
  // RequestParams object (containing the parameters the function was called
  // with, and a pointer to the underlying handler) and sends it to the
  // thread pool.
  void HandlerThreadPool::Wrapper::process_request(HttpStack::Request& req,
                                                   SAS::TrailId trail)
  {
    HandlerThreadPool::RequestParams* params =
      new HandlerThreadPool::RequestParams(_handler, req, trail);
    _pool->add_work(params);
  }

  // Implementation of HandlerInterface::sas_logger().  Simply call the
  // corresponding method on the underlying handler.
  HttpStack::SasLogger*
    HandlerThreadPool::Wrapper::sas_logger(HttpStack::Request& req)
  {
    return _handler->sas_logger(req);
  }

  //
  // Chronos utilities.
  //

  // Declaration of a logger for logging chronos flows.
  ChronosSasLogger CHRONOS_SAS_LOGGER;

  // LCOV_EXCL_START - Not currently unit tested as it is too closely coupled
  // with libevhtp.

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
                                          int target_latency,
                                          int current_latency,
                                          float rate_limit,
                                          uint32_t instance_id)
  {
    log_overload_event(trail,
                       req,
                       rc,
                       target_latency,
                       current_latency,
                       rate_limit,
                       instance_id,
                       SASEvent::HttpLogLevel::DETAIL);
  }
  // LCOV_EXCL_STOP

} // namespace HttpStackUtils
