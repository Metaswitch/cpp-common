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
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <poll.h>
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

#define HANDLE_NON_FD_CALL(FUNCTION, ...)                                      \
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

#define HANDLE_FD_CALL(FUNCTION, FD, ...)                                      \
  using FunctionType = decltype(&FUNCTION);                                    \
  static std::atomic<FunctionType> real_function(nullptr);                     \
  if (!real_function)                                                          \
  {                                                                            \
    real_function = (FunctionType)dlsym(RTLD_NEXT, #FUNCTION);                 \
  }                                                                            \
                                                                               \
  if ((fcntl((FD), F_GETFL) & O_NONBLOCK) == 0)                                \
  {                                                                            \
    maybe_abort();                                                             \
  }                                                                            \
                                                                               \
  return real_function(FD, __VA_ARGS__)

extern "C" ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
  HANDLE_FD_CALL(recv, sockfd, buf, len, flags);
}

extern "C" ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                            struct sockaddr *dest_addr, socklen_t* addrlen)
{
  HANDLE_FD_CALL(recvfrom, sockfd, buf, len, flags, dest_addr, addrlen);
}

extern "C" ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
{
  HANDLE_FD_CALL(recvmsg, sockfd, msg, flags);
}

extern "C" ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
  HANDLE_FD_CALL(send, sockfd, buf, len, flags);
}

extern "C" ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                          const struct sockaddr *dest_addr, socklen_t addrlen)
{
  HANDLE_FD_CALL(sendto, sockfd, buf, len, flags, dest_addr, addrlen);
}

extern "C" ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
  HANDLE_FD_CALL(sendmsg, sockfd, msg, flags);
}

extern "C" int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
  HANDLE_FD_CALL(connect, sockfd, addr, addrlen);
}

extern "C" int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  HANDLE_FD_CALL(accept, sockfd, addr, addrlen);
}

extern "C" int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
  HANDLE_FD_CALL(accept4, sockfd, addr, addrlen, flags);
}

extern "C" int select(int nfds, fd_set *readfds, fd_set *writefds,
                      fd_set *exceptfds, struct timeval *timeout)
{
  HANDLE_NON_FD_CALL(select, nfds, readfds, writefds, exceptfds, timeout);
}

extern "C" int pselect(int nfds, fd_set *readfds, fd_set *writefds,
                       fd_set *exceptfds, const struct timespec *timeout,
                       const sigset_t *sigmask)
{
  HANDLE_NON_FD_CALL(pselect, nfds, readfds, writefds, exceptfds, timeout, sigmask);
}

extern "C" int epoll_wait(int epfd, struct epoll_event *events,
                          int maxevents, int timeout)
{
  HANDLE_NON_FD_CALL(epoll_wait, epfd, events, maxevents, timeout);
}

extern "C" int epoll_pwait(int epfd, struct epoll_event *events,
                           int maxevents, int timeout,
                           const sigset_t *sigmask)
{
  HANDLE_NON_FD_CALL(epoll_pwait, epfd, events, maxevents, timeout, sigmask);
}

extern "C" int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
  HANDLE_NON_FD_CALL(poll, fds, nfds, timeout);
}

extern "C" int ppoll(struct pollfd *fds, nfds_t nfds,
                     const struct timespec *tmo_p, const sigset_t *sigmask)
{
  HANDLE_NON_FD_CALL(ppoll, fds, nfds, tmo_p, sigmask);
}
