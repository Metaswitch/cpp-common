/**
 * @file fakezmq.cpp 
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dlfcn.h>

#include "fakezmq.h"

static ZmqInterface* zmq_intf_p = NULL;

static void* (*real_zmq_ctx_new)(void) = NULL;
static void* (*real_zmq_socket)(void* context, int type) = NULL;
static int (*real_zmq_setsockopt)(void* s, int option, const void* optval, size_t optvallen) = NULL;
static int (*real_zmq_getsockopt)(void* s, int option, void* optval, size_t* optvallen) = NULL;
static int (*real_zmq_connect)(void* s, const char* addr) = NULL;
static int (*real_zmq_bind)(void* s, const char* addr) = NULL;
static int (*real_zmq_send)(void* s, const void* buf, size_t len, int flags) = NULL;
static int (*real_zmq_recv)(void* s, void* buf, size_t len, int flags) = NULL;
static int (*real_zmq_msg_recv)(zmq_msg_t* msg, void* s, int flags) = NULL;
static int (*real_zmq_close)(void* s) = NULL;
static int (*real_zmq_ctx_destroy)(void* context) = NULL;
static int (*real_zmq_msg_init)(zmq_msg_t* msg) = NULL;
static int (*real_zmq_msg_close)(zmq_msg_t* msg) = NULL;

ZmqInterface::ZmqInterface()
{
  pthread_mutex_init(&_mutex, NULL);

  pthread_condattr_t cond_attr;
  pthread_condattr_init(&cond_attr);
  pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);

  pthread_cond_init(&_cond, &cond_attr);

  pthread_condattr_destroy(&cond_attr);
}

bool ZmqInterface::call_complete(ZmqCall call, int timeout)
{
  struct timespec wake_time;
  int rc = 0;

  clock_gettime(CLOCK_MONOTONIC, &wake_time);
  wake_time.tv_sec += timeout;

  pthread_mutex_lock(&_mutex);
  while ((rc == 0) && (! _calls[call]))
  {
    rc = pthread_cond_timedwait(&_cond, &_mutex, &wake_time);
  } 
  _calls.reset(call);
  pthread_mutex_unlock(&_mutex);

  return (rc == 0);
}

void ZmqInterface::call_signal(ZmqCall call)
{
  pthread_mutex_lock(&_mutex);
  _calls.set(call);
  pthread_cond_signal(&_cond);
  pthread_mutex_unlock(&_mutex);
}

void cwtest_intercept_zmq(ZmqInterface* intf)
{
  zmq_intf_p = intf;
}

void cwtest_restore_zmq()
{
  zmq_intf_p = NULL;
}

void* zmq_ctx_new(void)
{
  void* ctx;

  if (!real_zmq_ctx_new)
  {
    real_zmq_ctx_new = (void*(*)(void))(intptr_t)dlsym(RTLD_NEXT, "zmq_ctx_new");
  }
  
  if (zmq_intf_p)
  {
    ctx = zmq_intf_p->zmq_ctx_new();

    zmq_intf_p->call_signal(ZmqInterface::ZMQ_CTX_NEW);
  }
  else
  {
    ctx = real_zmq_ctx_new();
  }

  return ctx;
}

void* zmq_socket(void* context, int type)
{
  void* s;

  if (!real_zmq_socket)
  {
    real_zmq_socket = (void*(*)(void* context, int type))(intptr_t)dlsym(RTLD_NEXT, "zmq_socket");
  }
  
  if (zmq_intf_p)
  {
    s = zmq_intf_p->zmq_socket(context, type);

    zmq_intf_p->call_signal(ZmqInterface::ZMQ_SOCKET);
  }
  else
  {
    s = real_zmq_socket(context, type);
  }

  return s;
}

int zmq_setsockopt(void* s, int option, const void* optval, size_t optvallen)
{
  int rc;

  if (!real_zmq_setsockopt)
  {
    real_zmq_setsockopt = (int(*)(void* s, int option, const void* optval, size_t optvallen))(intptr_t)dlsym(RTLD_NEXT, "zmq_setsockopt");
  }
  
  if (zmq_intf_p)
  {
    rc = zmq_intf_p->zmq_setsockopt(s, option, optval, optvallen);

    zmq_intf_p->call_signal(ZmqInterface::ZMQ_SETSOCKOPT);
  }
  else
  {
    rc = real_zmq_setsockopt(s, option, optval, optvallen);
  }

  return rc;
}

int zmq_getsockopt(void* s, int option, void* optval, size_t* optvallen)
{
  int rc;

  if (!real_zmq_getsockopt)
  {
    real_zmq_getsockopt = (int(*)(void* s, int option, void* optval, size_t* optvallen))(intptr_t)dlsym(RTLD_NEXT, "zmq_getsockopt");
  }
  
  if (zmq_intf_p)
  {
    rc = zmq_intf_p->zmq_getsockopt(s, option, optval, optvallen);

    zmq_intf_p->call_signal(ZmqInterface::ZMQ_GETSOCKOPT);
  }
  else
  {
    rc = real_zmq_getsockopt(s, option, optval, optvallen);
  }

  return rc;
}

int zmq_connect(void* s, const char* addr)
{
  int rc;

  if (!real_zmq_connect)
  {
    real_zmq_connect = (int(*)(void* s, const char* addr))(intptr_t)dlsym(RTLD_NEXT, "zmq_connect");
  }
  
  if (zmq_intf_p)
  {
    rc = zmq_intf_p->zmq_connect(s, addr);

    zmq_intf_p->call_signal(ZmqInterface::ZMQ_CONNECT);
  }
  else
  {
    rc = real_zmq_connect(s, addr);
  }

  return rc;
}

int zmq_bind(void* s, const char* addr)
{
  int rc;

  if (!real_zmq_bind)
  {
    real_zmq_bind = (int(*)(void* s, const char* addr))(intptr_t)dlsym(RTLD_NEXT, "zmq_bind");
  }
  
  if (zmq_intf_p)
  {
    rc = zmq_intf_p->zmq_bind(s, addr);

    zmq_intf_p->call_signal(ZmqInterface::ZMQ_BIND);
  }
  else
  {
    rc = real_zmq_bind(s, addr);
  }

  return rc;
}

int zmq_send(void* s, const void* buf, size_t len, int flags)
{
  int rc;

  if (!real_zmq_send)
  {
    real_zmq_send = (int(*)(void* s, const void* buf, size_t len, int flags))(intptr_t)dlsym(RTLD_NEXT, "zmq_send");
  }
  
  if (zmq_intf_p)
  {
    rc = zmq_intf_p->zmq_send(s, buf, len, flags);

    zmq_intf_p->call_signal(ZmqInterface::ZMQ_SEND);
  }
  else
  {
    rc = real_zmq_send(s, buf, len, flags);
  }

  return rc;
}

int zmq_recv(void* s, void* buf, size_t len, int flags)
{
  int rc;

  if (!real_zmq_recv)
  {
    real_zmq_recv = (int(*)(void* s, void* buf, size_t len, int flags))(intptr_t)dlsym(RTLD_NEXT, "zmq_recv");
  }
  
  if (zmq_intf_p)
  {
    rc = zmq_intf_p->zmq_recv(s, buf, len, flags);

    zmq_intf_p->call_signal(ZmqInterface::ZMQ_RECV);
  }
  else
  {
    rc = real_zmq_recv(s, buf, len, flags);
  }

  return rc;
}

int zmq_msg_recv(zmq_msg_t* msg, void* s, int flags)
{
  int rc;

  if (!real_zmq_msg_recv)
  {
    real_zmq_msg_recv = (int(*)(zmq_msg_t* msg, void* s, int flags))(intptr_t)dlsym(RTLD_NEXT, "zmq_msg_recv");
  }
  
  if (zmq_intf_p)
  {
    rc = zmq_intf_p->zmq_msg_recv(msg, s, flags);

    zmq_intf_p->call_signal(ZmqInterface::ZMQ_MSG_RECV);
  }
  else
  {
    rc = real_zmq_msg_recv(msg, s, flags);
  }

  return rc;
}

int zmq_close(void* s)
{
  int rc;

  if (!real_zmq_close)
  {
    real_zmq_close = (int(*)(void* s))(intptr_t)dlsym(RTLD_NEXT, "zmq_close");
  }
  
  if (zmq_intf_p)
  {
    rc = zmq_intf_p->zmq_close(s);

    zmq_intf_p->call_signal(ZmqInterface::ZMQ_CLOSE);
  }
  else
  {
    rc = real_zmq_close(s);
  }

  return rc;
}

int zmq_ctx_destroy(void* context)
{
  int rc;

  if (!real_zmq_ctx_destroy)
  {
    real_zmq_ctx_destroy = (int(*)(void* context))(intptr_t)dlsym(RTLD_NEXT, "zmq_ctx_destroy");
  }
  
  if (zmq_intf_p)
  {
    rc = zmq_intf_p->zmq_ctx_destroy(context);

    zmq_intf_p->call_signal(ZmqInterface::ZMQ_CTX_DESTROY);
  }
  else
  {
    rc = real_zmq_ctx_destroy(context);
  }

  return rc;
}

int zmq_msg_init(zmq_msg_t* msg)
{
  int rc;

  if (!real_zmq_msg_init)
  {
    real_zmq_msg_init = (int(*)(zmq_msg_t* msg))(intptr_t)dlsym(RTLD_NEXT, "zmq_msg_init");
  }
  
  if (zmq_intf_p)
  {
    real_zmq_msg_init(msg);

    rc = zmq_intf_p->zmq_msg_init(msg);

    zmq_intf_p->call_signal(ZmqInterface::ZMQ_MSG_INIT);
  }
  else
  {
    rc = real_zmq_msg_init(msg);
  }

  return rc;
}

int zmq_msg_close(zmq_msg_t* msg)
{
  int rc;

  if (!real_zmq_msg_close)
  {
    real_zmq_msg_close = (int(*)(zmq_msg_t* msg))(intptr_t)dlsym(RTLD_NEXT, "zmq_msg_close");
  }
  
  if (zmq_intf_p)
  {
    rc = zmq_intf_p->zmq_msg_close(msg);

    zmq_intf_p->call_signal(ZmqInterface::ZMQ_MSG_CLOSE);
  }
  else
  {
    rc = real_zmq_msg_close(msg);
  }

  return rc;
}

