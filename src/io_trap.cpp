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

// LCOV_EXCL_START

// C header files must be included with C linkage to prevent name mangling.
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


/// Function that is called when the library is loaded.
static void on_library_load() __attribute__((constructor));

void on_library_load() {
  // Unset the LD_LIBRARY_PATH variable.
  //
  // This is needed because the library relies other symbols in cpp-common that
  // are provided by the program being debugged.  If the library didn't unset
  // the LD_LIBRARY_PATH it would get inherited by any subprocess spawned by the
  // program (e.g.  when launching gdb to gather a stack trace). But since the
  // subprocess doesn't have the relevant symbols it won't launch correctly.
  printf("*** IO trap loaded ***\n");
  printf("Unsetting LD_PRELOAD environment variable\n");
  unsetenv("LD_PRELOAD");
}


/// Function that is called when a thread is about to make a syscall that could
/// block (e.g. is a socket is not in a particular state).
static void about_to_block()
{
  // If the thread has not notified the IO monitor that it is about to do IO,
  // and it is not allowed to do IO without notifying the IO monitor, we abort
  // to generate a call stack and a core file.
  if (!Utils::IOMonitor::thread_doing_overt_io() &&
      !Utils::IOMonitor::thread_allows_covert_io())
  {
    fprintf(stderr, "Trapping disallowed I/o - abort\n"); fflush(stderr);
    abort();
  }
}


// Helper macro to call the real underlying function and return from the
// surrounding function.
//
// @param [in] FUNCTION - The function being interposed.
// @param [in[ ...      - Arguments to call the real function with.
#define RETURN_CALL_REAL_FUNCTION(FUNCTION, ...)                               \
  do {                                                                         \
    using FunctionType = decltype(&FUNCTION);                                  \
    static std::atomic<FunctionType> func(nullptr);                            \
    if (!func)                                                                 \
    {                                                                          \
      func = (FunctionType)dlsym(RTLD_NEXT, #FUNCTION);                        \
    }                                                                          \
    return (*func)(__VA_ARGS__);                                               \
  } while (0)


// Macro to commonalise the code to handle a system call that waits on multiple
// file descriptors (e.g. poll, epoll, select).
//
// This macro does the following:
//
// -  Stores off a pointer to the real function being interposed.
// -  Calls a function to handle the consequences of this function blocking.
// -  Calls the real function.
//
// @param [in] FUNCTION - The function being interposed.
// @param [in] ...      - The arguments passed to the function.
#define HANDLE_NON_FD_CALL(FUNCTION, ...)                                      \
  do {                                                                         \
    about_to_block();                                                          \
    RETURN_CALL_REAL_FUNCTION(FUNCTION, __VA_ARGS__);                          \
  } while (0)


// Macro to commonalise the code to handle a potentially blocking system call
// that operates on a single file descriptor (e.g. connect, send).
//
// This macro works the same as the HANDLE_NON_FD_CALL macro except it first
// checks if the socket is in non-blocking mode before calling about_to_block.
//
// @param [in] FUNCTION - The function being interposed.
// @param [in] FD       - The file descriptor being operated on.
// @param [in] ...      - The remaining arguments passed to the function
//                        (beyond the file descriptor).
#define HANDLE_FD_CALL(FUNCTION, FD, ...)                                      \
  do {                                                                         \
    if ((fcntl((FD), F_GETFL) & O_NONBLOCK) == 0)                              \
    {                                                                          \
      about_to_block();                                                        \
    }                                                                          \
                                                                               \
    RETURN_CALL_REAL_FUNCTION(FUNCTION, FD, __VA_ARGS__);                      \
  } while (0)


//
// Interpose functions that might do IO. These all have C-style linkage to
// ensure the symbols in the resulting library have the right name.
//

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

// Some versions of gcc define poll in a header file, and this calls into
// functions like __poll and __poll_chk. We need to interpose these as we can't
// interpose the poll function in this case (as linkage to poll will happen at
// compile-time, not link-time).
extern "C" int __poll_chk(struct pollfd *fds, nfds_t nfds, int timeout, __SIZE_TYPE__ fds_len)
{
  HANDLE_NON_FD_CALL(__poll_chk, fds, nfds, timeout, fds_len);
}

extern "C" int __poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
  HANDLE_NON_FD_CALL(__poll, fds, nfds, timeout);
}

// LCOV_EXCL_STOP
