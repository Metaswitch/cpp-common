/**
 * @file fakezmq.cpp 
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

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dlfcn.h>

#include "fakezmq.h"

static ZmqInterface* zmq_intf_p = NULL;

static void* (*real_zmq_ctx_new)(void);
static void* (*real_zmq_socket)(void* context, int type);
static int (*real_zmq_setsockopt)(void* s, int option, const void* optval, size_t optvallen);
static int (*real_zmq_connect)(void* s, const char* addr);
static int (*real_zmq_send)(void* s, const void* buf, size_t len, int flags);
static int (*real_zmq_recv)(void* s, void* buf, size_t len, int flags);
static int (*real_zmq_close)(void* s);
static int (*real_zmq_ctx_destroy)(void* context);

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

