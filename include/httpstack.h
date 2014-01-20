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

#include "accesslogger.h"

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
    Request(HttpStack* stack, evhtp_request_t* req) : _stack(stack), _req(req) {}
    inline std::string path() {return url_unescape(std::string(_req->uri->path->path));}
    inline std::string full_path() {return url_unescape(std::string(_req->uri->path->full));}
    inline std::string file() {return url_unescape(std::string((_req->uri->path->file != NULL) ? _req->uri->path->file : ""));}
    inline std::string param(const std::string& name)
    {
      const char* param = evhtp_kv_find(_req->uri->query, name.c_str());
      return url_unescape(std::string(param != NULL ? param : ""));
    }
    void add_content(const std::string& content) {evbuffer_add(_req->buffer_out, content.c_str(), content.length());}
    void send_reply(int rc);

  private:
    HttpStack* _stack;
    evhtp_request_t* _req;
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
    inline Handler(Request& req) : _req(req) {}
    virtual ~Handler() {}

    virtual void run() = 0;

  protected:
    Request _req;
  };

  class BaseHandlerFactory
  {
  public:
    BaseHandlerFactory() {}
    virtual Handler* create(Request& req) = 0;
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

  static inline HttpStack* get_instance() {return INSTANCE;};
  virtual void initialize();
  virtual void configure(const std::string& bind_address,
                         unsigned short port,
                         int num_threads,
                         AccessLogger* access_logger = NULL);
  virtual void register_handler(char* path, BaseHandlerFactory* factory);
  virtual void start();
  virtual void stop();
  virtual void wait_stopped();

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
  void event_base_thread_fn();

  // Don't implement the following, to avoid copies of this instance.
  HttpStack(HttpStack const&);
  void operator=(HttpStack const&);

  std::string _bind_address;
  unsigned short _bind_port;
  int _num_threads;
  AccessLogger* _access_logger;
  evbase_t* _evbase;
  evhtp_t* _evhtp;
  pthread_t _event_base_thread;
};

#endif
