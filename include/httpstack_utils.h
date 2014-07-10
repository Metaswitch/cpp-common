/**
 * @file httpstack_utils.h Utilities for use with the HttpStack
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

#ifndef HTTPSTACK_UTILS_H__
#define HTTPSTACK_UTILS_H__

#include "httpstack.h"
#include "threadpool.h"

namespace HttpStackUtils
{
  /// @class Handler
  ///
  /// Base class for per-request handler objects.
  class Handler
  {
  public:
    inline Handler(HttpStack::Request& req, SAS::TrailId trail) :
      _req(req), _trail(trail)
    {}

    virtual ~Handler() {}

    /// Process the request associated with this handler. Subclasses of this
    /// class should implement it with their specific business logic.
    virtual void run() = 0;

  protected:
    /// Send an HTTP reply. Calls through to Request::send_reply, picking up
    /// the trail ID from the handler.
    ///
    /// @param status_code the HTTP status code to use on the reply.
    inline void send_http_reply(int status_code)
    {
      _req.send_reply(status_code, trail());
    }

    /// @return the trail ID associated with the request.
    inline SAS::TrailId trail() { return _trail; }

    /// Record a penalty with the load monitor.  This is used to apply
    /// backpressure in the event of overload of a downstream device.
    inline void record_penalty() { _req.record_penalty(); }

    HttpStack::Request _req;
    SAS::TrailId _trail;
  };

  /// @class ControllerThreadPool
  ///
  /// The HttpStack has a limited number of transport threads so controllers
  /// must take care not to block them while doing external work.  This class
  /// is a thread pool that allows the application to execute certain
  /// handlers in a worker thread (which is allowed to block).
  ///
  /// Example code:
  ///   HttpStackUtils::ControllerThreadPool pool(50);
  ///
  ///   ExampleConfig config;
  ///   SpawningController<ExampleHandler, ExampleConfig> controller1(&config,
  ///                                                                 &pool)
  class ControllerThreadPool : public ThreadPool<Handler*>
  {
  public:
    ControllerThreadPool(unsigned int num_threads,
                         unsigned int max_queue = 0);

    void process_work(Handler*& params);
  };

  /// @class SpawningController
  ///
  /// Many controllers use an asynchronous non-blocking execution model.
  /// Instead of blocking the current thread when doing external operations,
  /// they register callbacks that are called (potentially on a different
  /// thread) when the operation completes.  These controllers create a new
  /// "handler" object per request that tracks the state necessary to continue
  /// processing when the callback is triggered.
  ///
  /// This class is an implementation of the controller part of this model.
  ///
  /// It takes two template parameters:
  /// @tparam H the type of the handler.
  /// @tparam C Although not mandatory according to the ControllerInterface, in
  ///   practice all controllers have some sort of associated config. This is
  ///   the type of the config object.
  template<class H, class C>
  class SpawningController : public HttpStack::ControllerInterface
  {
  public:
    inline SpawningController(const C* cfg,
                              ControllerThreadPool* pool = NULL) :
      _cfg(cfg), _pool(pool)
    {}

    virtual ~SpawningController() {}

    /// Process an HTTP request by spawning a new handler object and running it
    /// (either in the current thread or the attached thread pool).
    ///
    /// @param req the request to process.
    /// @param trail the SAS trail ID for the request.
    void process_request(HttpStack::Request& req, SAS::TrailId trail)
    {
      Handler* handler = new H(req, _cfg, trail);

      if (_pool != NULL)
      {
        _pool->add_work(handler);
      }
      else
      {
        handler->run();
      }
    }

  private:
    const C* _cfg;
    ControllerThreadPool* _pool;
  };

  /// @class PingController
  ///
  /// Simple controller that receives ping requests and responds to them.
  class PingController : public HttpStack::ControllerInterface
  {
    void process_request(HttpStack::Request& req, SAS::TrailId trail);
  };

  // Implementation of an HttpStack SAS logger for logging chronos flows. This
  // logs all transactions at "detail" level (level 40).
  class ChronosSasLogger : public HttpStack::SasLogger
  {
    void sas_log_rx_http_req(SAS::TrailId trail,
                             HttpStack::Request& req,
                             uint32_t instance_id = 0);

    void sas_log_tx_http_rsp(SAS::TrailId trail,
                             HttpStack::Request& req,
                             int rc,
                             uint32_t instance_id = 0);

    void sas_log_overload(SAS::TrailId trail,
                          HttpStack::Request& req,
                          int rc,
                          uint32_t instance_id = 0);
  };

  extern ChronosSasLogger CHRONOS_SAS_LOGGER;

} // namespace HttpStackUtils

#endif

