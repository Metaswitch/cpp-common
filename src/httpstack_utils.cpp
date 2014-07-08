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

  //
  // ControllerThreadPool methods.
  //
  ControllerThreadPool::ControllerThreadPool(unsigned int num_threads,
                                             unsigned int max_queue) :
    _pool(num_threads, max_queue), _wrappers()
  {
    _pool.start();
  }

  HttpStack::ControllerInterface*
    ControllerThreadPool::wrap(HttpStack::ControllerInterface* controller)
  {
    // Create a new wrapper around the specifiec controller and record it in
    // the wrappers vector.
    Wrapper* wrapper = new Wrapper(&_pool, controller);
    _wrappers.push_back(wrapper);
    return wrapper;
  }

  ControllerThreadPool::~ControllerThreadPool()
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

  ControllerThreadPool::Pool::Pool(unsigned int num_threads,
                                   unsigned int max_queue) :
    ThreadPool<RequestParams*>(num_threads, max_queue)
  {}

  // This function defines how the worker threads process received requests.
  // This is simply to invoke the process_request() method on the underlying
  // controller.
  void ControllerThreadPool::Pool::
    process_work(HttpStackUtils::ControllerThreadPool::RequestParams*& params)
  {
    params->controller->process_request(params->request, params->trail);
    delete params; params = NULL;
  }

  ControllerThreadPool::Wrapper::Wrapper(Pool* pool,
                                         ControllerInterface* controller) :
    _pool(pool), _controller(controller)
  {}

  // Implementation of ControllerInterface::process_request().  This builds a
  // RequestParams object (containing the parameters the function was called
  // with, and a pointer to the underlying controller) and sends it to the
  // thread pool.
  void ControllerThreadPool::Wrapper::process_request(HttpStack::Request& req,
                                                      SAS::TrailId trail)
  {
    ControllerThreadPool::RequestParams* params =
      new ControllerThreadPool::RequestParams(_controller, req, trail);
    _pool->add_work(params);
  }

  // Implementation of ControllerInterface::sas_log_level().  Simply call the
  // corresponding method on the underlying controller.
  SASEvent::HttpLogLevel
    ControllerThreadPool::Wrapper::sas_log_level(HttpStack::Request& req)
  {
    return _controller->sas_log_level(req);
  }

} // namespace HttpStackUtils
