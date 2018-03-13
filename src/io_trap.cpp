/**
 * @file io_trap.cpp
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <dlfcn.h>
}

#include <atomic>
#include "utils.h"

static void maybe_abort()
{
  // Abort if we want to trap unexpected IO, and this IO is unexpected.
  if (!Utils::IOMonitor::thread_doing_overt_io() &&
      !Utils::IOMonitor::thread_allows_covert_io())
  {
    abort();
  }
}

#define INTERPOSE(FUNCTION, ...)                                               \
  using FunctionType = decltype(&FUNCTION);                                    \
  static std::atomic<FunctionType> real_function(nullptr);                     \
  if (!real_function)                                                          \
  {                                                                            \
    real_function = (FunctionType)dlsym(RTLD_NEXT, #FUNCTION);                 \
  }                                                                            \
                                                                               \
  maybe_abort();                                                               \
                                                                               \
  return real_function(__VA_ARGS__)


extern "C" ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
  INTERPOSE(recv, sockfd, buf, len, flags);
}

extern "C" ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                            struct sockaddr *dest_addr, socklen_t* addrlen)
{
  INTERPOSE(recvfrom, sockfd, buf, len, flags, dest_addr, addrlen);
}

extern "C" ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
{
  INTERPOSE(recvmsg, sockfd, msg, flags);
}

extern "C" ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
  INTERPOSE(send, sockfd, buf, len, flags);
}

extern "C" ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                          const struct sockaddr *dest_addr, socklen_t addrlen)
{
  INTERPOSE(sendto, sockfd, buf, len, flags, dest_addr, addrlen);
}

extern "C" ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
  INTERPOSE(sendmsg, sockfd, msg, flags);
}

extern "C" int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
  INTERPOSE(connect, sockfd, addr, addrlen);
}

extern "C" int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  INTERPOSE(accept, sockfd, addr, addrlen);
}

extern "C" int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
  INTERPOSE(accept4, sockfd, addr, addrlen, flags);
}
