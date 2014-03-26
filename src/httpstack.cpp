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

#include "httpstack.h"
#include <cstring>
#include "log.h"
#include "sasevent.h"

HttpStack* HttpStack::INSTANCE = &DEFAULT_INSTANCE;
HttpStack HttpStack::DEFAULT_INSTANCE;
bool HttpStack::_ev_using_pthreads = false;

HttpStack::HttpStack() :
  _access_logger(NULL),
  _stats(NULL),
  _load_monitor(NULL)
{}

void HttpStack::Request::send_reply(int rc, SAS::TrailId trail)
{
  stopwatch.stop();
  _stack->send_reply(*this, rc, trail);
}

bool HttpStack::Request::get_latency(unsigned long& latency_us)
{
  return stopwatch.read(latency_us);
}

void HttpStack::send_reply(Request& req, int rc, SAS::TrailId trail)
{
  LOG_VERBOSE("Sending response %d to request for URL %s, args %s", rc, req.req()->uri->path->full, req.req()->uri->query_raw);

  log(std::string(req.req()->uri->path->full), rc);
  sas_log_tx_http_rsp(trail, req, rc, 0);

  evhtp_send_reply(req.req(), rc);

  // Resume the request to actually send it.  This matches the function to pause the request in
  // HttpStack::handler_callback_fn.
  evhtp_request_resume(req.req());

  // Update the latency stats and throttling algorithm.
  unsigned long latency_us = 0;
  if (req.get_latency(latency_us))
  {
    if (_load_monitor != NULL)
    {
      _load_monitor->request_complete(latency_us);
    }

    if (_stats != NULL)
    {
      _stats->update_http_latency_us(latency_us);
    }
  }
}

void HttpStack::initialize()
{
  // Initialize if we haven't already done so.  We don't do this in the
  // constructor because we can't throw exceptions on failure.
  if (!_ev_using_pthreads)
  {
    // Tell libevent to use pthreads.  If you don't, it seems to disable
    // locking, with hilarious results.
    evthread_use_pthreads();
    _ev_using_pthreads = true;
  }

  if (!_evbase)
  {
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
                          AccessLogger* access_logger,
                          HttpStack::StatsInterface* stats,
                          LoadMonitor* load_monitor)
{
  LOG_STATUS("Configuring HTTP stack");
  LOG_STATUS("  Bind address: %s", bind_address.c_str());
  LOG_STATUS("  Bind port:    %u", bind_port);
  LOG_STATUS("  Num threads:  %d", num_threads);
  _bind_address = bind_address;
  _bind_port = bind_port;
  _num_threads = num_threads;
  _access_logger = access_logger;
  _stats = stats;
  _load_monitor = load_monitor;
}

void HttpStack::register_handler(char* path, HttpStack::BaseHandlerFactory* factory)
{
  evhtp_callback_t* cb = evhtp_set_regex_cb(_evhtp, path, handler_callback_fn, (void*)factory);
  if (cb == NULL)
  {
    throw Exception("evhtp_set_cb", 0); // LCOV_EXCL_LINE
  }
}

void HttpStack::start(evhtp_thread_init_cb init_cb)
{
  initialize();

  int rc = evhtp_use_threads(_evhtp, init_cb, _num_threads, this);
  if (rc != 0)
  {
    throw Exception("evhtp_use_threads", rc); // LCOV_EXCL_LINE
  }

  // If the bind address is IPv6 evhtp needs to be told by prepending ipv6 to
  // the  parameter.  Use getaddrinfo() to analyse the bind_address.
  addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;    // To allow both IPv4 and IPv6 addresses.
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* servinfo = NULL;

  std::string full_bind_address = _bind_address;
  const int error_num = getaddrinfo(_bind_address.c_str(), NULL, &hints, &servinfo);

  if ((error_num == 0) &&
      (servinfo->ai_family == AF_INET6))
  {
    full_bind_address = "ipv6:" + full_bind_address;
  }

  freeaddrinfo(servinfo);

  rc = evhtp_bind_socket(_evhtp, full_bind_address.c_str(), _bind_port, 1024);
  if (rc != 0)
  {
    throw Exception("evhtp_bind_socket", rc); // LCOV_EXCL_LINE
  }

  rc = pthread_create(&_event_base_thread, NULL, event_base_thread_fn, this);
  if (rc != 0)
  {
    throw Exception("pthread_create", rc); // LCOV_EXCL_LINE
  }
}

void HttpStack::stop()
{
  LOG_STATUS("Stopping HTTP stack");
  event_base_loopbreak(_evbase);
  evhtp_unbind_socket(_evhtp);
}

void HttpStack::wait_stopped()
{
  LOG_STATUS("Waiting for HTTP stack to stop");
  pthread_join(_event_base_thread, NULL);
  evhtp_free(_evhtp);
  _evhtp = NULL;
  event_base_free(_evbase);
  _evbase = NULL;
}

void HttpStack::handler_callback_fn(evhtp_request_t* req, void* handler_factory)
{
  INSTANCE->handler_callback(req, (HttpStack::BaseHandlerFactory*)handler_factory);
}

void HttpStack::handler_callback(evhtp_request_t* req,
                                 HttpStack::BaseHandlerFactory* handler_factory)
{
  Request request(this, req);

  SAS::TrailId trail = SAS::new_trail(0);
  sas_log_rx_http_req(trail, request, handler_factory, 0);

  if (_stats != NULL)
  {
    _stats->incr_http_incoming_requests();
  }

  if ((_load_monitor == NULL) || _load_monitor->admit_request())
  {
    // Pause the request processing (which stops it from being cancelled), as we
    // may process this request asynchronously.  The
    // HttpStack::Request::send_reply method resumes.
    evhtp_request_pause(req);

    // Create a Request and a Handler and kick off processing.
    LOG_VERBOSE("Handling request for URL %s, args %s", req->uri->path->full, req->uri->query_raw);
    Handler* handler = handler_factory->create(request);
    handler->set_trail(trail);
    handler->run();
  }
  else
  {
    sas_log_overload(trail, request, 503, 0);
    evhtp_send_reply(req, 503);

    if (_stats != NULL)
    {
      _stats->incr_http_rejected_overload();
    }
  }
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

void HttpStack::record_penalty()
{
  if (_load_monitor != NULL)
  {
    _load_monitor->incr_penalties();
  }
}

std::string HttpStack::Request::body()
{
  std::string body = "";
  char buf[1024];
  int bytes;
  while (evbuffer_get_length(_req->buffer_in) > 0)
  {
    bytes = evbuffer_remove(_req->buffer_in, buf, 1024);
    body.append(buf, bytes);
  }
  return body;
}

void HttpStack::sas_log_rx_http_req(SAS::TrailId trail,
                                    HttpStack::Request& req,
                                    HttpStack::BaseHandlerFactory* handler_factory,
                                    uint32_t instance_id)
{
  std::string correlator = req.header(SASEvent::HTTP_BRANCH_HEADER_NAME);
  if (correlator != "")
  {
    SAS::Marker corr_marker(trail, MARKER_ID_VIA_BRANCH_PARAM, instance_id);
    corr_marker.add_var_param(correlator);
    SAS::report_marker(corr_marker, SAS::Marker::Scope::Trace);
  }

  SAS::Event rx_http_req(trail, SASEvent::RX_HTTP_REQ, instance_id);
  rx_http_req.add_static_param(req.req()->method);
  rx_http_req.add_var_param(req.full_path());
  rx_http_req.add_var_param(req.body());
  SAS::report_event(rx_http_req);
}

void HttpStack::sas_log_tx_http_rsp(SAS::TrailId trail,
                                    HttpStack::Request& req,
                                    int rc,
                                    uint32_t instance_id)
{
  SAS::Event tx_http_rsp(trail, SASEvent::TX_HTTP_RSP, instance_id);
  tx_http_rsp.add_static_param(rc);
  tx_http_rsp.add_static_param(req.req()->method);
  tx_http_rsp.add_var_param(req.full_path());

  // The response body is stored in an evbuffer in libevhtp.
  uint8_t buffer[0xFFFF];
  ev_ssize_t buffer_len;
  buffer_len = evbuffer_copyout(req.req()->buffer_out, buffer, sizeof(buffer));
  tx_http_rsp.add_var_param(buffer_len, buffer);

  SAS::report_event(tx_http_rsp);
}

void HttpStack::sas_log_overload(SAS::TrailId trail,
                                 HttpStack::Request& req,
                                 int rc,
                                 uint32_t instance_id)
{
  SAS::Event event(trail, SASEvent::HTTP_REJECTED_OVERLOAD, instance_id);
  event.add_static_param(rc);
  event.add_static_param(req.req()->method);
  event.add_var_param(req.full_path());
  SAS::report_event(event);
}
