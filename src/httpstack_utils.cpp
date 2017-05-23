/**
 * @file httpstack_utils.cpp Utility classes and functions for use with the
 * HttpStack.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
    req.set_track_latency(false);
    req.send_reply(200, trail);
  }

  //
  // HandlerThreadPool methods.
  //
  HandlerThreadPool::HandlerThreadPool(unsigned int num_threads,
                                       ExceptionHandler* exception_handler,
                                       unsigned int max_queue) :
    _pool(num_threads, 
          exception_handler, 
          &exception_callback, 
          max_queue),
    _wrappers()
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
                                ExceptionHandler* exception_handler,
                                void (*callback)(HttpStackUtils::HandlerThreadPool::RequestParams*),
                                unsigned int max_queue) :
    ThreadPool<RequestParams*>(num_threads, exception_handler, callback, max_queue)
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
