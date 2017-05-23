/**
 * @file fakezmq.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
    ZMQ_GETSOCKOPT,
    ZMQ_CONNECT,
    ZMQ_BIND,
    ZMQ_SEND,
    ZMQ_RECV,
    ZMQ_MSG_RECV,
    ZMQ_CLOSE,
    ZMQ_CTX_DESTROY,
    ZMQ_MSG_INIT,
    ZMQ_MSG_CLOSE,
    ZMQ_NUM_CALLS
  };

  ZmqInterface();

  virtual void* zmq_ctx_new(void) = 0;
  virtual void* zmq_socket(void* context, int type) = 0;
  virtual int zmq_setsockopt(void* s, int option, const void* optval, size_t optvallen) = 0;
  virtual int zmq_getsockopt(void* s, int option, void* optval, size_t* optvallen) = 0;
  virtual int zmq_connect(void* s, const char* addr) = 0;
  virtual int zmq_bind (void* s, const char* addr) = 0;
  virtual int zmq_send(void* s, const void* buf, size_t len, int flags) = 0;
  virtual int zmq_recv(void* s, void* buf, size_t len, int flags) = 0;
  virtual int zmq_msg_recv(zmq_msg_t* msg, void* s, int flags) = 0;
  virtual int zmq_close(void* s) = 0;
  virtual int zmq_ctx_destroy(void* context) = 0;
  virtual int zmq_msg_init(zmq_msg_t* msg) = 0;
  virtual int zmq_msg_close(zmq_msg_t* msg) = 0;

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
  MOCK_METHOD4(zmq_getsockopt, int(void* s, int option, void* optval, size_t* optvallen));
  MOCK_METHOD2(zmq_connect, int(void* s, const char* addr));
  MOCK_METHOD2(zmq_bind, int(void* s, const char* addr));
  MOCK_METHOD4(zmq_send, int(void* s, const void *buf, size_t len, int flags));
  MOCK_METHOD4(zmq_recv, int(void* s, void* buf, size_t len, int flags));
  MOCK_METHOD3(zmq_msg_recv, int(zmq_msg_t* msg, void* s, int flags));
  MOCK_METHOD1(zmq_close, int(void* s));
  MOCK_METHOD1(zmq_ctx_destroy, int(void* context));
  MOCK_METHOD1(zmq_msg_init, int(zmq_msg_t* msg));
  MOCK_METHOD1(zmq_msg_close, int(zmq_msg_t* msg));
};

void cwtest_intercept_zmq(ZmqInterface* intf);
void cwtest_restore_zmq();

