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

#include <evhtp.h>

#include "utils.h"
#include "accesslogger.h"
#include "load_monitor.h"
#include "sas.h"
#include "sasevent.h"

class HttpStack
{
public:
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
    _method(htp_method_UNKNOWN), _body_set(false), _stack(stack), _req(req), stopwatch()
    {
      stopwatch.start();
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

    htp_method method()
    {
      if (_method == htp_method_UNKNOWN)
      {
        _method = evhtp_request_get_method(_req);
      };
      return _method;
    }

    std::string body();

    void send_reply(int rc, SAS::TrailId trail);
    inline evhtp_request_t* req() { return _req; }

    void record_penalty() { _stack->record_penalty(); }


    /// Get the latency of the request.
    ///
    /// @param latency_us The latency of the request in microseconds.  Only
    /// valid if the fucntion returns true.
    ///
    /// @return Whether the latency has been successfully obtained.
    bool get_latency(unsigned long& latency_us);

    void set_sas_log_level(SASEvent::HttpLogLevel level) { _sas_log_level = level; }
    SASEvent::HttpLogLevel get_sas_log_level() { return _sas_log_level; }

  protected:
    htp_method _method;
    std::string _body;
    bool _body_set;

  private:
    HttpStack* _stack;
    evhtp_request_t* _req;
    Utils::StopWatch stopwatch;
    SASEvent::HttpLogLevel _sas_log_level;

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
  };

  class Handler
  {
  public:
    inline Handler(Request& req, SAS::TrailId trail) : _req(req), _trail(trail)
    virtual ~Handler() {}

    virtual void run() = 0;

    /// Send an HTTP reply. Calls through to Request::send_reply, picking up
    /// the trail ID from the handler.
    ///
    /// @param status_code the HTTP status code to use on the reply.
    void send_http_reply(int status_code) { _req.send_reply(status_code, trail()); }

    inline SAS::TrailId trail() { return _trail; }

  protected:
    void record_penalty() { _req.record_penalty(); }

    SAS::TrailId _trail;
    Request _req;
  };

  class BaseHandlerFactory
  {
  public:
    BaseHandlerFactory() {}
    virtual Handler* create(Request& req) = 0;

    /// Return the level to log the HTTP transaction at for a given request.
    /// This default implementation logs everything at protocol level (60).
    virtual SASEvent::HttpLogLevel sas_log_level(Request& req)
    {
      return SASEvent::HttpLogLevel::PROTOCOL;
    }
  };

  template <class H>
  class HandlerFactory : public BaseHandlerFactory
  {
  public:
    HandlerFactory() : BaseHandlerFactory() {}
    Handler* create(Request& req) { return new H(req); }
  };

  template <class H, class C>
  class ConfiguredHandlerFactory : public BaseHandlerFactory
  {
  public:
    ConfiguredHandlerFactory(const C* cfg) : BaseHandlerFactory(), _cfg(cfg) {}
    Handler* create(Request& req) { return new H(req, _cfg); }
  private:
    const C* _cfg;
  };

  class StatsInterface
  {
  public:
    virtual void update_http_latency_us(unsigned long latency_us) = 0;
    virtual void incr_http_incoming_requests() = 0;
    virtual void incr_http_rejected_overload() = 0;
  };

  static inline HttpStack* get_instance() {return INSTANCE;};
  virtual void initialize();
  virtual void configure(const std::string& bind_address,
                         unsigned short port,
                         int num_threads,
                         AccessLogger* access_logger = NULL,
                         StatsInterface* stats = NULL,
                         LoadMonitor* load_monitor = NULL);
  virtual void register_handler(char* path, BaseHandlerFactory* factory);
  virtual void start(evhtp_thread_init_cb init_cb = NULL);
  virtual void stop();
  virtual void wait_stopped();
  virtual void send_reply(Request& req, int rc, SAS::TrailId trail);
  virtual void record_penalty();

  void log(const std::string uri, int rc)
  {
    if (_access_logger)
    {
      _access_logger->log(uri, rc);
    }
  };

private:
  static HttpStack* INSTANCE;
  static HttpStack DEFAULT_INSTANCE;

  HttpStack();
  virtual ~HttpStack() {}
  static void handler_callback_fn(evhtp_request_t* req, void* handler_factory);
  static void* event_base_thread_fn(void* http_stack_ptr);
  void handler_callback(evhtp_request_t* req,
                        BaseHandlerFactory* handler_factory);
  void event_base_thread_fn();
  void sas_log_rx_http_req(SAS::TrailId trail,
                           Request& req,
                           BaseHandlerFactory* handler_factory,
                           uint32_t instance_id);
  void sas_log_tx_http_rsp(SAS::TrailId trail,
                           HttpStack::Request& req,
                           int rc,
                           uint32_t instance_id);
  void sas_log_overload(SAS::TrailId trail,
                        HttpStack::Request& req,
                        int rc,
                        uint32_t instance_id);
  // Don't implement the following, to avoid copies of this instance.
  HttpStack(HttpStack const&);
  void operator=(HttpStack const&);

  std::string _bind_address;
  unsigned short _bind_port;
  int _num_threads;

  AccessLogger* _access_logger;
  StatsInterface* _stats;
  LoadMonitor* _load_monitor;

  evbase_t* _evbase;
  evhtp_t* _evhtp;
  pthread_t _event_base_thread;

  static bool _ev_using_pthreads;
};

#endif
