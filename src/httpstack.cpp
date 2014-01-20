/**
 * @file httpstack.cpp class implementation wrapping libevhtp HTTP stack
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

#include <httpstack.h>

HttpStack* HttpStack::INSTANCE = &DEFAULT_INSTANCE;
HttpStack HttpStack::DEFAULT_INSTANCE;

HttpStack::HttpStack() {}

void HttpStack::Request::send_reply(int rc)
{
  // Log and set up the return code.
  _stack->log(std::string(_req->uri->path->full), rc);
  evhtp_send_reply(_req, rc);

  // Resume the request to actually send it.  This matches the function to pause the request in
  // HttpStack::handler_callback_fn.
  evhtp_request_resume(_req);
}

void HttpStack::initialize()
{
  // Initialize if we haven't already done so.  We don't do this in the
  // constructor because we can't throw exceptions on failure.
  if (!_evbase)
  {
    // Tell libevent to use pthreads.  If you don't, it seems to disable locking, with hilarious
    // results.
    evthread_use_pthreads();
    _evbase = event_base_new();
  }
  if (!_evhtp)
  {
    _evhtp = evhtp_new(_evbase, NULL);
  }
}

void HttpStack::configure(const std::string& bind_address,
                          unsigned short bind_port,
                          int num_threads,
                          AccessLogger* access_logger)
{
  _bind_address = bind_address;
  _bind_port = bind_port;
  _num_threads = num_threads;
  _access_logger = access_logger;
}

void HttpStack::register_handler(char* path, HttpStack::BaseHandlerFactory* factory)
{
  evhtp_callback_t* cb = evhtp_set_regex_cb(_evhtp, path, handler_callback_fn, (void*)factory);
  if (cb == NULL)
  {
    throw Exception("evhtp_set_cb", 0);
  }
}

void HttpStack::start()
{
  initialize();

  int rc = evhtp_use_threads(_evhtp, NULL, _num_threads, this);
  if (rc != 0)
  {
    throw Exception("evhtp_use_threads", rc);
  }

  rc = evhtp_bind_socket(_evhtp, _bind_address.c_str(), _bind_port, 1024);
  if (rc != 0)
  {
    throw Exception("evhtp_bind_socket", rc);
  }

  rc = pthread_create(&_event_base_thread, NULL, event_base_thread_fn, this);
  if (rc != 0)
  {
    throw Exception("pthread_create", rc);
  }
}

void HttpStack::stop()
{
  event_base_loopbreak(_evbase);
  evhtp_unbind_socket(_evhtp);
}

void HttpStack::wait_stopped()
{
  pthread_join(_event_base_thread, NULL);
}

void HttpStack::handler_callback_fn(evhtp_request_t* req, void* handler_factory)
{
  // Pause the request processing (which stops it from being cancelled), as we may process this
  // request asynchronously.  The HttpStack::Request::send_reply method resumes.
  evhtp_request_pause(req);

  // Create a Request and a Handler and kick off processing.
  Request request(INSTANCE, req);
  Handler* handler = ((HttpStack::BaseHandlerFactory*)handler_factory)->create(request);
  handler->run();
}

void* HttpStack::event_base_thread_fn(void* http_stack_ptr)
{
  ((HttpStack*)http_stack_ptr)->event_base_thread_fn();
  return NULL;
}

void HttpStack::event_base_thread_fn()
{
  event_base_loop(_evbase, 0);
}
