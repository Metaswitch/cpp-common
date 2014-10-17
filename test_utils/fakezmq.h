/**
 * @file fakezmq.h
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014  Metaswitch Networks Ltd
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

#pragma once

#include <zmq.h>
#include <pthread.h>

#include <bitset>

#include "gmock/gmock.h"

class ZmqInterface
{
public:
  enum ZmqCall
  {
    ZMQ_CTX_NEW,
    ZMQ_SOCKET,
    ZMQ_SETSOCKOPT,
    ZMQ_CONNECT,
    ZMQ_SEND,
    ZMQ_RECV,
    ZMQ_CLOSE,
    ZMQ_CTX_DESTROY,
    ZMQ_NUM_CALLS
  };

  ZmqInterface();

  virtual void* zmq_ctx_new(void) = 0;
  virtual void* zmq_socket(void* context, int type) = 0;
  virtual int zmq_setsockopt(void* s, int option, const void* optval, size_t optvallen) = 0;
  virtual int zmq_connect(void* s, const char* addr) = 0;
  virtual int zmq_send(void* s, const void* buf, size_t len, int flags) = 0;
  virtual int zmq_recv(void* s, void* buf, size_t len, int flags) = 0;
  virtual int zmq_close(void* s) = 0;
  virtual int zmq_ctx_destroy(void* context) = 0;

  bool call_complete(ZmqCall call, int timeout);
  void call_signal(ZmqCall call);

private:
  pthread_mutex_t _mutex;
  pthread_cond_t  _cond;

  std::bitset<ZMQ_NUM_CALLS> _calls;
};

class MockZmqInterface : public ZmqInterface
{
public:
  MOCK_METHOD0(zmq_ctx_new, void*(void));
  MOCK_METHOD2(zmq_socket, void*(void* context, int type));
  MOCK_METHOD4(zmq_setsockopt, int(void* s, int option, const void* optval, size_t optvallen));
  MOCK_METHOD2(zmq_connect, int(void* s, const char* addr));
  MOCK_METHOD4(zmq_send, int(void* s, const void *buf, size_t len, int flags));
  MOCK_METHOD4(zmq_recv, int(void* s, void* buf, size_t len, int flags));
  MOCK_METHOD1(zmq_close, int(void* s));
  MOCK_METHOD1(zmq_ctx_destroy, int(void* context));
};

void cwtest_intercept_zmq(ZmqInterface* intf);
void cwtest_restore_zmq();

