/**
 * @file httpstack.h class definitition wrapping HTTP stack
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

#ifndef HTTP_H__
#define HTTP_H__

#include <pthread.h>
#include <string>
#include <set>

#include <evhtp.h>

#include "utils.h"
#include "accesslogger.h"
#include "load_monitor.h"
#include "sas.h"
#include "sasevent.h"
#include "exception_handler.h"

class HttpStack
{
public:
  class SasLogger;

  class Exception
  {
  public:
    inline Exception(const char* func, int rc) : _func(func), _rc(rc) {};
    const char* _func;
    const int _rc;
  };

  class Request
  {
  public:
    Request(HttpStack* stack, evhtp_request_t* req) :
      _method(htp_method_UNKNOWN),
      _rx_body_set(false),
      _req(req),
      _stack(stack),
      _stopwatch(),
      _track_latency(true)
    {
      _stopwatch.start();
    }
    virtual ~Request() {};

    inline std::string path()
    {
      return url_unescape(std::string(_req->uri->path->path));
    }

    inline std::string full_path()
    {
      return url_unescape(std::string(_req->uri->path->full));
    }

    inline std::string file()
    {
      return url_unescape(std::string((_req->uri->path->file != NULL) ?
                                        _req->uri->path->file : ""));
    }

    inline std::string param(const std::string& name)
    {
      const char* param = evhtp_kv_find(_req->uri->query, name.c_str());
      return url_unescape(std::string(param != NULL ? param : ""));
    }

    inline std::string header(const std::string& name)
    {
      const char* val = evhtp_header_find(_req->headers_in, name.c_str());
      return std::string((val != NULL ? val : ""));
    }

    void add_content(const std::string& content)
    {
      evbuffer_add(_req->buffer_out, content.c_str(), content.length());
    }

    void add_header(const std::string& name, const std::string& value)
    {
      evhtp_header_t* new_header = evhtp_header_new(name.c_str(),
                                                    value.c_str(),
                                                    1, 1);
      evhtp_headers_add_header(_req->headers_out, new_header);
    }

    inline void set_track_latency(bool track_latency)
    {
      _track_latency = track_latency;
    }

    htp_method method()
    {
      if (_method == htp_method_UNKNOWN)
      {
        _method = evhtp_request_get_method(_req);
      };
      return _method;
    }

    std::string method_as_str()
    {
      switch (method())
      {
      case htp_method_GET: return "GET";
      case htp_method_HEAD: return "HEAD";
      case htp_method_POST: return "POST";
      case htp_method_PUT: return "PUT";
      case htp_method_DELETE: return "DELETE";
      case htp_method_MKCOL: return "MKCOL";
      case htp_method_COPY: return "COPY";
      case htp_method_MOVE: return "MOVE";
      case htp_method_OPTIONS: return "OPTIONS";
      case htp_method_PROPFIND: return "PROPFIND";
      case htp_method_PROPPATCH: return "PROPPATCH";
      case htp_method_LOCK: return "LOCK";
      case htp_method_UNLOCK: return "UNLOCK";
      case htp_method_TRACE: return "TRACE";
      case htp_method_CONNECT: return "CONNECT";
      case htp_method_PATCH: return "PATCH";
      case htp_method_UNKNOWN: return "(unknown method)";
      default:
        return std::string("htp_method " + std::to_string(method()));
      }
    }

    void send_reply(int rc, SAS::TrailId trail);
    inline evhtp_request_t* req() { return _req; }

    void record_penalty() { _stack->record_penalty(); }

    std::string get_rx_message();
    std::string get_rx_header();
    std::string get_rx_body();

    std::string get_tx_message(int rc);
    std::string get_tx_header(int rc);
    std::string get_tx_body();

    bool get_remote_ip_port(std::string& ip, unsigned short& port);
    bool get_local_ip_port(std::string& ip, unsigned short& port);

    /// Get the latency of the request.
    ///
    /// @param latency_us The latency of the request in microseconds.  Only
    /// valid if the fucntion returns true.
    ///
    /// @return Whether the latency has been successfully obtained.
    bool get_latency(unsigned long& latency_us);

    void set_sas_logger(SasLogger* logger) { _sas_logger = logger; }

    inline void sas_log_rx_http_req(SAS::TrailId trail,
                                    uint32_t instance_id = 0)
    {
      _sas_logger->sas_log_rx_http_req(trail, *this, instance_id);
    }

    inline void sas_log_tx_http_rsp(SAS::TrailId trail,
                                    int rc,
                                    uint32_t instance_id = 0)
    {
      _sas_logger->sas_log_tx_http_rsp(trail, *this, rc, instance_id);
    }

    inline void sas_log_overload(SAS::TrailId trail,
                                 int rc,
                                 int target_latency,
                                 int current_latency,
                                 float rate_limit,
                                 uint32_t instance_id = 0)
    {
      _sas_logger->sas_log_overload(trail,
                                    *this,
                                    rc,
                                    target_latency,
                                    current_latency,
                                    rate_limit,
                                    instance_id);
    }

  protected:
    htp_method _method;
    std::string _rx_body;
    bool _rx_body_set;
    evhtp_request_t* _req;

  private:
    HttpStack* _stack;
    Utils::StopWatch _stopwatch;
    SasLogger* _sas_logger;
    bool _track_latency;

    std::string url_unescape(const std::string& s)
    {
      std::string r;
      r.reserve(2*s.length());
      char a, b;
      for (size_t ii = 0; ii < s.length(); ++ii)
      {
        if ((s[ii] == '%') && ((a = s[ii+1]) & (b = s[ii+2])) && (isxdigit(a) && isxdigit(b)))
        {
          if (a >= 'a') a -= 'a'-'A';
          if (a >= 'A') a -= ('A' - 10);
          else a -= '0';
          if (b >= 'a') b -= 'a'-'A';
          if (b >= 'A') b -= ('A' - 10);
          else b -= '0';
          r.push_back(16*a+b);
          ii+=2;
        }
        else
        {
          r.push_back(s[ii]);
        }
      }
      return r;
    }

    /// Utility method to convert an evbuffer to a C++ string.
    ///
    /// @param eb  - The evbuffer to convert
    /// @return    - A string containing a copy of the contents of the evbuffer.
    static std::string evbuffer_to_string(evbuffer* eb);
  };

  class SasLogger
  {
  public:
    // Log a received HTTP request.
    //
    // @param trail SAS trail ID to log on.
    // @param req request to log.
    // @instance_id unique instance ID for the event.
    virtual void sas_log_rx_http_req(SAS::TrailId trail,
                                     Request& req,
                                     uint32_t instance_id = 0) = 0;

    // Log a transmitted HTTP response.
    //
    // @param trail SAS trail ID to log on.
    // @param req request to log.
    // @param rc the HTTP response code.
    // @instance_id unique instance ID for the event.
    virtual void sas_log_tx_http_rsp(SAS::TrailId trail,
                                     Request& req,
                                     int rc,
                                     uint32_t instance_id = 0) = 0;

    // Log when an HTTP request is rejected due to overload.
    //
    // @param trail SAS trail ID to log on.
    // @param req request to log.
    // @param rc the HTTP response code.
    // @instance_id unique instance ID for the event.
    virtual void sas_log_overload(SAS::TrailId trail,
                                  Request& req,
                                  int rc,
                                  int target_latency,
                                  int current_latency,
                                  float rate_limit,
                                  uint32_t instance_id = 0) = 0;

  protected:
    //
    // Utility methods.
    //

    // Log any correlating markers encoded in the message header.
    void log_correlator(SAS::TrailId trail, Request& req, uint32_t instance_id);

    // Log that a request has been received using the normal SAS event IDs.
    void log_req_event(SAS::TrailId trail,
                       Request& req,
                       uint32_t instance_id,
                       SASEvent::HttpLogLevel level = SASEvent::HttpLogLevel::PROTOCOL);

    // Log that a response has been sent using the normal SAS event IDs.
    void log_rsp_event(SAS::TrailId trail,
                       Request& req,
                       int rc,
                       uint32_t instance_id,
                       SASEvent::HttpLogLevel level = SASEvent::HttpLogLevel::PROTOCOL,
                       bool omit_body = false);

    // Log that a request has been rejected due to overload, using the normal
    // SAS event IDs.
    void log_overload_event(SAS::TrailId trail,
                            Request& req,
                            int rc,
                            int target_latency,
                            int current_latency,
                            float rate_limit,
                            uint32_t instance_id,
                            SASEvent::HttpLogLevel level = SASEvent::HttpLogLevel::PROTOCOL);

    // Add the remote and local IP addresses and ports to an event.
    void add_ip_addrs_and_ports(SAS::Event& event, Request& req);
  };

  /// Default implementation of SAS Logger.  Logs with default severity.
  class DefaultSasLogger : public SasLogger
  {
  public:
    void sas_log_rx_http_req(SAS::TrailId trail,
                             Request& req,
                             uint32_t instance_id = 0);
    void sas_log_tx_http_rsp(SAS::TrailId trail,
                             Request& req,
                             int rc,
                             uint32_t instance_id = 0);
    void sas_log_overload(SAS::TrailId trail,
                          Request& req,
                          int rc,
                          int target_latency,
                          int current_latency,
                          float rate_limit,
                          uint32_t instance_id = 0);
  };

  /// "Null" SAS Logger.  Does not log.
  class NullSasLogger : public SasLogger
  {
  public:
    // Implement the API but don't actually log.
    void sas_log_rx_http_req(SAS::TrailId trail,
                             Request& req,
                             uint32_t instance_id = 0) {}
    void sas_log_tx_http_rsp(SAS::TrailId trail,
                             Request& req,
                             int rc,
                             uint32_t instance_id = 0) {}
    void sas_log_overload(SAS::TrailId trail,
                          Request& req,
                          int rc,
                          int target_latency,
                          int current_latency,
                          float rate_limit,
                          uint32_t instance_id = 0) {}
  };

  class HandlerInterface
  {
  public:
    /// Process a new HTTP request.
    ///
    /// @param req the object representing the request.  This function does not
    ///   take ownership of the request - implementations of this function must
    ///   take a copy if they wish to reference it outside of the current call
    ///   stack.
    /// @param trail the SAS trail ID associated with the reqeust.
    virtual void process_request(Request& req, SAS::TrailId trail) = 0;

    /// Get the instance of the SasLogger that this handler uses to log HTTP
    /// transactions.
    ///
    /// The default implemention returns the default logger
    /// (HttpStack::SAS_LOGGER).
    ///
    /// @param req the transaction to log.
    /// @return the object used to log the transaction.
    virtual SasLogger* sas_logger(Request& req)
    {
      return &HttpStack::DEFAULT_SAS_LOGGER;
    }
  };

  class StatsInterface
  {
  public:
    virtual void update_http_latency_us(unsigned long latency_us) = 0;
    virtual void incr_http_incoming_requests() = 0;
    virtual void incr_http_rejected_overload() = 0;
  };

  HttpStack(int num_threads,
            ExceptionHandler* exception_handler,
            AccessLogger* access_logger = NULL,
            LoadMonitor* load_monitor = NULL,
            StatsInterface* stats = NULL);
  virtual ~HttpStack();

  virtual void initialize();
  virtual void bind_tcp_socket(const std::string& bind_address,
                               unsigned short port);
  virtual void bind_unix_socket(const std::string& bind_path);
  virtual void register_handler(const char* path, HandlerInterface* handler);
  virtual void register_default_handler(HandlerInterface* handler);
  virtual void start(evhtp_thread_init_cb init_cb = NULL);
  virtual void stop();
  virtual void wait_stopped();
  virtual void send_reply(Request& req, int rc, SAS::TrailId trail);
  virtual void record_penalty();

  void log(const std::string uri, std::string method, int rc, unsigned long latency_us)
  {
    if (_access_logger)
    {
      _access_logger->log(uri, method, rc, latency_us);
    }
  };

  static DefaultSasLogger DEFAULT_SAS_LOGGER;
  static NullSasLogger NULL_SAS_LOGGER;

private:
  virtual void send_reply_internal(Request& req, int rc, SAS::TrailId trail);
  static void handler_callback_fn(evhtp_request_t* req, void* handler_reg_param);
  static void* event_base_thread_fn(void* http_stack_ptr);
  void handler_callback(evhtp_request_t* req, HandlerInterface* handler);
  void event_base_thread_fn();

  // Don't implement the following, to avoid copies of this instance.
  HttpStack(HttpStack const&);
  void operator=(HttpStack const&);

  int _num_threads;

  ExceptionHandler* _exception_handler;
  AccessLogger* _access_logger;
  LoadMonitor* _load_monitor;
  StatsInterface* _stats;

  evbase_t* _evbase;
  evhtp_t* _evhtp;
  pthread_t _event_base_thread;

  static bool _ev_using_pthreads;

  // Helper structure used to register handlers with libevhtp, while also
  // allowing callbacks to get back to the HttpStack object.
  struct HandlerRegistration
  {
    HttpStack* stack;
    HandlerInterface* handler;

    HandlerRegistration() : HandlerRegistration(nullptr, nullptr) {}
    HandlerRegistration(HttpStack* stack_param, HandlerInterface* handler_param) :
      stack(stack_param), handler(handler_param)
    {}
  };

  // Active handler registrations - stored here so they can be freed when the
  // stack is destroyed.
  std::set<HandlerRegistration*> _handler_registrations;
};

#endif
