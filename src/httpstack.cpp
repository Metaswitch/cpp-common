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
#include <sys/stat.h>
#include "log.h"

bool HttpStack::_ev_using_pthreads = false;
HttpStack::DefaultSasLogger HttpStack::DEFAULT_SAS_LOGGER;
HttpStack::NullSasLogger HttpStack::NULL_SAS_LOGGER;

HttpStack::HttpStack(int num_threads,
                     ExceptionHandler* exception_handler,
                     AccessLogger* access_logger,
                     LoadMonitor* load_monitor,
                     StatsInterface* stats) :
  _num_threads(num_threads),
  _exception_handler(exception_handler),
  _access_logger(access_logger),
  _load_monitor(load_monitor),
  _stats(stats),
  _evbase(nullptr),
  _evhtp(nullptr)
{
  TRC_STATUS("Constructing HTTP stack with %d threads", _num_threads);
}

HttpStack::~HttpStack()
{
  for(std::set<HandlerRegistration*>::iterator reg = _handler_registrations.begin();
      reg != _handler_registrations.end();
      ++reg)
  {
    delete *reg;
  }
}

void HttpStack::Request::send_reply(int rc, SAS::TrailId trail)
{
  _stopwatch.stop();
  _stack->send_reply(*this, rc, trail);
}

bool HttpStack::Request::get_latency(unsigned long& latency_us)
{
  return ((_track_latency) && (_stopwatch.read(latency_us)));
}

// Wrapper around evhtp_send_reply to ensure that all responses are
// logged to the access log and SAS.
void HttpStack::send_reply_internal(Request& req, int rc, SAS::TrailId trail)
{
  TRC_VERBOSE("Sending response %d to request for URL %s, args %s", rc, req.req()->uri->path->full, req.req()->uri->query_raw);
  unsigned long latency_us = 0;
  req.get_latency(latency_us);
  log(std::string(req.req()->uri->path->full), req.method_as_str(), rc, latency_us);
  req.sas_log_tx_http_rsp(trail, rc, 0);

  evhtp_send_reply(req.req(), rc);
}


void HttpStack::send_reply(Request& req,
                           int rc,
                           SAS::TrailId trail)
{
  send_reply_internal(req, rc, trail);
  // Resume the request to actually send it.  This matches the function to pause the request in
  // HttpStack::handler_callback_fn.
  evhtp_request_resume(req.req());

  // Update the latency stats and throttling algorithm if it's appropriate for
  // the request
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

void HttpStack::register_handler(const char* path,
                                 HttpStack::HandlerInterface* handler)
{
  HandlerRegistration* reg = new HandlerRegistration(this, handler);
  _handler_registrations.insert(reg);

  evhtp_callback_t* cb = evhtp_set_regex_cb(_evhtp,
                                            path,
                                            handler_callback_fn,
                                            (void*)reg);
  if (cb == NULL)
  {
    throw Exception("evhtp_set_cb", 0); // LCOV_EXCL_LINE
  }
}

void HttpStack::register_default_handler(HttpStack::HandlerInterface* handler)
{
  HandlerRegistration* reg = new HandlerRegistration(this, handler);
  _handler_registrations.insert(reg);

  evhtp_set_gencb(_evhtp,
                  handler_callback_fn,
                  (void*)reg);
}

void HttpStack::bind_tcp_socket(const std::string& bind_address,
                                unsigned short port)
{
  TRC_STATUS("Binding HTTP TCP socket: address=%s, port=%d", bind_address.c_str(), port);

  // If the bind address is IPv6 evhtp needs to be told by prepending ipv6 to
  // the  parameter.  Use getaddrinfo() to analyse the bind_address.
  addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;    // To allow both IPv4 and IPv6 addresses.
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* servinfo = NULL;

  std::string full_bind_address = bind_address;
  std::string local_bind_address = "127.0.0.1";
  const int error_num = getaddrinfo(bind_address.c_str(), NULL, &hints, &servinfo);

  if ((error_num == 0) &&
      (servinfo->ai_family == AF_INET))
  {
    char dest_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,
              &(((struct sockaddr_in*)servinfo->ai_addr)->sin_addr),
              dest_str,
              INET_ADDRSTRLEN);
    full_bind_address = dest_str;
  }
  else if ((error_num == 0) &&
           (servinfo->ai_family == AF_INET6))
  {
    char dest_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6,
              &(((struct sockaddr_in6*)servinfo->ai_addr)->sin6_addr),
              dest_str,
              INET6_ADDRSTRLEN);
    full_bind_address = dest_str;
    full_bind_address = "ipv6:" + full_bind_address;
    local_bind_address = "ipv6:::1";
  }

  freeaddrinfo(servinfo);

  int rc = evhtp_bind_socket(_evhtp, full_bind_address.c_str(), port, 1024);
  if (rc != 0)
  {
    // LCOV_EXCL_START
    TRC_ERROR("evhtp_bind_socket failed with address %s and port %d",
              full_bind_address.c_str(),
              port);
    throw Exception("evhtp_bind_socket (tcp)", rc);
    // LCOV_EXCL_STOP
  }

  if ((local_bind_address != full_bind_address) &&
      (full_bind_address != "0.0.0.0")          &&
      (full_bind_address != "ipv6:::"))
  {
    // Listen on the local address as well as the main address (so long as the
    // main address isn't all)
    rc = evhtp_bind_socket(_evhtp, local_bind_address.c_str(), port, 1024);
    if (rc != 0)
    {
      // LCOV_EXCL_START
      TRC_ERROR("evhtp_bind_socket failed with address %s and port %d",
                local_bind_address.c_str(),
                port);
      throw Exception("evhtp_bind_socket (tcp) - localhost", rc);
      // LCOV_EXCL_STOP
    }
  }
}

void HttpStack::bind_unix_socket(const std::string& bind_path)
{
  TRC_STATUS("Binding HTTP unix socket: path=%s", bind_path.c_str());

  // libevhtp does not correctly remove any old socket before creating a new
  // one, so we have to do this ourselves.
  ::remove(bind_path.c_str());

  std::string full_bind_address = "unix:" + bind_path;

  int rc = evhtp_bind_socket(_evhtp, full_bind_address.c_str(), 0, 1024);
  if (rc != 0)
  {
    // LCOV_EXCL_START
    TRC_ERROR("evhtp_bind_socket failed with path %s",
              full_bind_address.c_str());
    throw Exception("evhtp_bind_socket (unix)", rc);
    // LCOV_EXCL_STOP
  }

  // Socket needs to be world-writeable for nginx to use it
  chmod(bind_path.c_str(), 0777);
}

void HttpStack::start(evhtp_thread_init_cb init_cb)
{
  int rc = evhtp_use_threads(_evhtp, init_cb, _num_threads, this);
  if (rc != 0)
  {
    throw Exception("evhtp_use_threads", rc); // LCOV_EXCL_LINE
  }

  rc = pthread_create(&_event_base_thread, NULL, event_base_thread_fn, this);
  if (rc != 0)
  {
    // LCOV_EXCL_START
    TRC_ERROR("pthread_create failed in HTTPStack creation");
    throw Exception("pthread_create", rc);
    // LCOV_EXCL_STOP
  }
}

void HttpStack::stop()
{
  TRC_STATUS("Stopping HTTP stack");
  event_base_loopbreak(_evbase);
  evhtp_unbind_socket(_evhtp);
}

void HttpStack::wait_stopped()
{
  TRC_STATUS("Waiting for HTTP stack to stop");
  pthread_join(_event_base_thread, NULL);
  evhtp_free(_evhtp);
  _evhtp = NULL;
  event_base_free(_evbase);
  _evbase = NULL;
}

void HttpStack::handler_callback_fn(evhtp_request_t* req, void* handler_reg_param)
{
  HandlerRegistration* handler_reg =
    static_cast<HandlerRegistration*>(handler_reg_param);
  handler_reg->stack->handler_callback(req, handler_reg->handler);
}

void HttpStack::handler_callback(evhtp_request_t* req,
                                 HttpStack::HandlerInterface* handler)
{
  Request request(this, req);

  // Call into the handler to request a SAS logger that can be used to log
  // this request.  Then actually log the request.
  request.set_sas_logger(handler->sas_logger(request));

  SAS::TrailId trail = SAS::new_trail(0);
  request.sas_log_rx_http_req(trail, 0);

  if (_stats != NULL)
  {
    _stats->incr_http_incoming_requests();
  }

  if ((_load_monitor == NULL) || _load_monitor->admit_request(trail))
  {
    // Pause the request processing (which stops it from being cancelled), as we
    // may process this request asynchronously.  The
    // HttpStack::Request::send_reply method resumes.
    evhtp_request_pause(req);

    // Pass the request to the handler.
    TRC_VERBOSE("Process request for URL %s, args %s",
                req->uri->path->full,
                req->uri->query_raw);

    CW_TRY
    {
      handler->process_request(request, trail);
    }
    // LCOV_EXCL_START
    CW_EXCEPT(_exception_handler)
    {
      send_reply_internal(request, 500, trail);

      if (_num_threads == 1)
      {
        // There's only one HTTP thread, so we can't sensibly proceed.
        exit(1);
      }
    }
    CW_END
    // LCOV_EXCL_STOP
  }
  else
  {
    request.sas_log_overload(trail,
                             503,
                             _load_monitor->get_target_latency(),
                             _load_monitor->get_current_latency(),
                             _load_monitor->get_rate_limit(),
                             0);
    send_reply_internal(request, 503, trail);

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

std::string HttpStack::Request::get_rx_body()
{
  if (!_rx_body_set)
  {
    _rx_body = evbuffer_to_string(_req->buffer_in);
  }
  return _rx_body;
}

std::string HttpStack::Request::get_tx_body()
{
  return evbuffer_to_string(_req->buffer_out);
}

std::string HttpStack::Request::get_rx_header()
{
  return evbuffer_to_string(_req->header_buffer_in);
}

std::string HttpStack::Request::get_tx_header(int rc)
{
  std::string hdr;
  evbuffer* eb = evbuffer_new();

  if (evhtp_get_response_header(_req, rc, eb) == 0)
  {
    hdr = evbuffer_to_string(eb);
  }

  evbuffer_free(eb);
  return hdr;
}

std::string HttpStack::Request::get_rx_message()
{
  return get_rx_header() + get_rx_body();
}

std::string HttpStack::Request::get_tx_message(int rc)
{
  return get_tx_header(rc) + get_tx_body();
}

std::string HttpStack::Request::evbuffer_to_string(evbuffer* eb)
{
  std::string s;
  size_t len = evbuffer_get_length(eb);
  void* buf = evbuffer_pullup(eb, len);

  if (buf != NULL)
  {
    s.assign((char*)buf, len);
  }

  return s;
}


bool HttpStack::Request::get_remote_ip_port(std::string& ip, unsigned short& port)
{
  bool rc = false;
  char ip_buf[64];

  if (evhtp_get_remote_ip_port(evhtp_request_get_connection(_req),
                               ip_buf,
                               sizeof(ip_buf),
                               &port) == 0)
  {
    ip.assign(ip_buf);
    rc = true;
  }
  return rc;
}

bool HttpStack::Request::get_local_ip_port(std::string& ip, unsigned short& port)
{
  bool rc = false;
  char ip_buf[64];

  if (evhtp_get_local_ip_port(evhtp_request_get_connection(_req),
                              ip_buf,
                              sizeof(ip_buf),
                              &port) == 0)
  {
    ip.assign(ip_buf);
    rc = true;
  }
  return rc;
}

//
// SasLogger methods.
//

void HttpStack::SasLogger::log_correlator(SAS::TrailId trail,
                                          Request& req,
                                          uint32_t instance_id)
{
  std::string correlator = req.header(SASEvent::HTTP_BRANCH_HEADER_NAME);
  if (correlator != "")
  {
    SAS::Marker corr_marker(trail, MARKER_ID_VIA_BRANCH_PARAM, instance_id);
    corr_marker.add_var_param(correlator);

    // Report a correlating marker to SAS.  Set the option that means any
    // associations will not reactivate the trail group.  Otherwise
    // interactions with this server that happen after the call ends will cause
    // long delays in the call appearing in SAS.
    SAS::report_marker(corr_marker, SAS::Marker::Scope::Trace, false);
  }
}

void HttpStack::SasLogger::log_req_event(SAS::TrailId trail,
                                         Request& req,
                                         uint32_t instance_id,
                                         SASEvent::HttpLogLevel level)
{
  int event_id = ((level == SASEvent::HttpLogLevel::PROTOCOL) ?
                  SASEvent::RX_HTTP_REQ : SASEvent::RX_HTTP_REQ_DETAIL);
  SAS::Event event(trail, event_id, instance_id);

  add_ip_addrs_and_ports(event, req);
  event.add_static_param(req.method());
  event.add_var_param(req.full_path());
  event.add_compressed_param(req.get_rx_message(), &SASEvent::PROFILE_HTTP);

  SAS::report_event(event);
}

void HttpStack::SasLogger::log_rsp_event(SAS::TrailId trail,
                                         Request& req,
                                         int rc,
                                         uint32_t instance_id,
                                         SASEvent::HttpLogLevel level,
                                         bool omit_body)
{
  int event_id = ((level == SASEvent::HttpLogLevel::PROTOCOL) ?
                  SASEvent::TX_HTTP_RSP : SASEvent::TX_HTTP_RSP_DETAIL);
  SAS::Event event(trail, event_id, instance_id);

  add_ip_addrs_and_ports(event, req);
  event.add_static_param(req.method());
  event.add_static_param(rc);
  event.add_var_param(req.full_path());

  if (!omit_body)
  {
    event.add_compressed_param(req.get_tx_message(rc), &SASEvent::PROFILE_HTTP);
  }
  else
  {
    // LCOV_EXCL_START
    if (req.get_tx_body().empty())
    {
      // We are omitting the body but there wasn't one in the messaage. Just log
      // the header.
      event.add_compressed_param(req.get_tx_header(rc), &SASEvent::PROFILE_HTTP);
    }
    else
    {
      // There was a body that we need to omit. Add a fake body to the header
      // explaining that the body was intentionally not logged.
      event.add_compressed_param(req.get_tx_header(rc) + "<Body present but not logged>",
                                 &SASEvent::PROFILE_HTTP);
    }
    // LCOV_EXCL_STOP
  }

  SAS::report_event(event);
}

void HttpStack::SasLogger::log_overload_event(SAS::TrailId trail,
                                              Request& req,
                                              int rc,
                                              int target_latency,
                                              int current_latency,
                                              float rate_limit,
                                              uint32_t instance_id,
                                              SASEvent::HttpLogLevel level)
{
  int event_id = ((level == SASEvent::HttpLogLevel::PROTOCOL) ?
                  SASEvent::HTTP_REJECTED_OVERLOAD :
                  SASEvent::HTTP_REJECTED_OVERLOAD_DETAIL);
  SAS::Event event(trail, event_id, instance_id);
  event.add_static_param(req.method());
  event.add_static_param(rc);
  event.add_static_param(target_latency);
  event.add_static_param(current_latency);
  event.add_static_param(rate_limit);
  event.add_var_param(req.full_path());
  SAS::report_event(event);
}

void HttpStack::SasLogger::add_ip_addrs_and_ports(SAS::Event& event, Request& req)
{
  std::string ip;
  unsigned short port;

  if (req.get_remote_ip_port(ip, port))
  {
    event.add_var_param(ip);
    event.add_static_param(port);
  }
  else
  {
    // LCOV_EXCL_START
    event.add_var_param("unknown");
    event.add_static_param(0);
    // LCOV_EXCL_STOP
  }

  if (req.get_local_ip_port(ip, port))
  {
    event.add_var_param(ip);
    event.add_static_param(port);
  }
  else
  {
    // LCOV_EXCL_START
    event.add_var_param("unknown");
    event.add_static_param(0);
    // LCOV_EXCL_STOP
  }
}

//
// DefaultSasLogger methods.
//

void HttpStack::DefaultSasLogger::sas_log_rx_http_req(SAS::TrailId trail,
                                                      HttpStack::Request& req,
                                                      uint32_t instance_id)
{
  log_correlator(trail, req, instance_id);
  log_req_event(trail, req, instance_id);
}


void HttpStack::DefaultSasLogger::sas_log_tx_http_rsp(SAS::TrailId trail,
                                                      HttpStack::Request& req,
                                                      int rc,
                                                      uint32_t instance_id)
{
  log_rsp_event(trail, req, rc, instance_id);
}

void HttpStack::DefaultSasLogger::sas_log_overload(SAS::TrailId trail,
                                                   HttpStack::Request& req,
                                                   int rc,
                                                   int target_latency,
                                                   int current_latency,
                                                   float rate_limit,
                                                   uint32_t instance_id)
{
  log_overload_event(trail, req, rc, target_latency, current_latency, rate_limit, instance_id);
}

