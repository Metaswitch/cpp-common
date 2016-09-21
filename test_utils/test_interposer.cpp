/**
 * @file test_interposer.hpp Unit test interposer - hooks various calls that are useful for UT.
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


#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <dlfcn.h>
#include <pthread.h>

#include <map>
#include <string>
#include <cstdio>
#include <cerrno>

#include "log.h"
#include "test_interposer.hpp"

/// The map we use.
static std::map<std::string, std::string> host_map;

/// Lock protecting access to our time control structures.
static pthread_mutex_t time_lock = PTHREAD_MUTEX_INITIALIZER;

/// The current time offset.  Protected by time_lock.
static struct timespec time_offset = { 0, 0 };

// Whether time is completely controlled by the test script. Protected by
// time_lock.
static bool completely_control_time = false;

/// When completely controlling time, these store the time at which the test
/// scripts took control. Protected by time_lock.
static const clockid_t supported_clock_ids[] = {CLOCK_REALTIME,
                                                CLOCK_REALTIME_COARSE,
                                                CLOCK_MONOTONIC,
                                                CLOCK_MONOTONIC_COARSE};
static std::map<clockid_t, struct timespec> abs_timespecs;
static time_t abs_time;

// Whether file opening is completely controlled by the test script.
static bool control_fopen = false;

// When controlling fopen, this stores the file pointer to return.
static FILE* fopen_file_pointer = NULL;

/// The real functions we are interposing.
static int (*real_getaddrinfo)(const char*, const char*, const struct addrinfo*, struct addrinfo**);
static struct hostent* (*real_gethostbyname)(const char*);
static int (*real_clock_gettime)(clockid_t, struct timespec *);
static time_t (*real_time)(time_t*);
static FILE* (*real_fopen)(const char*, const char*);
typedef int (*pthread_cond_timedwait_func_t)(pthread_cond_t*,
                                             pthread_mutex_t*,
                                             const struct timespec*);
pthread_cond_timedwait_func_t real_pthread_cond_timedwait;

/// Helper: add two timespecs. Arbitrary aliasing is fine.
static inline void ts_add(struct timespec& a, struct timespec& b, struct timespec& res)
{
  long nsec = a.tv_nsec + b.tv_nsec;
  res.tv_nsec = nsec % (1000L * 1000L * 1000L);
  res.tv_sec = (time_t)(nsec / (1000L * 1000L * 1000L)) + a.tv_sec + b.tv_sec;
}

/// Helper: subtract timespec b from timespec a. Arbitrary aliasing is fine.
static inline void ts_sub(const struct timespec& a, const struct timespec& b, struct timespec& res)
{
  int carry;
  if (a.tv_nsec > b.tv_nsec)
  {
    res.tv_nsec = a.tv_nsec - b.tv_nsec;
    carry = 0;
  }
  else
  {
    res.tv_nsec = (1000L * 1000L * 1000L) - b.tv_nsec + a.tv_nsec;
    carry = 1;
  }
  res.tv_sec = (time_t)a.tv_sec - b.tv_sec - carry;
}

/// Add a new mapping: lookup for host will actually lookup target.
void cwtest_add_host_mapping(std::string host, std::string target)
{
  host_map[host] = target;
}

/// Clear all mappings.
void cwtest_clear_host_mapping()
{
  host_map.clear();
}

/// Alter the fabric of space-time.
void cwtest_advance_time_ms(long delta_ms)  ///< Delta to add to the current offset applied to returned times (in ms).
{
  struct timespec delta = { delta_ms / 1000, (delta_ms % 1000) * 1000L * 1000L };
  pthread_mutex_lock(&time_lock);
  ts_add(time_offset, delta, time_offset);
  pthread_mutex_unlock(&time_lock);
}

/// Restore the fabric of space-time.
void cwtest_reset_time()
{
  pthread_mutex_lock(&time_lock);
  time_offset.tv_sec = 0;
  time_offset.tv_nsec = 0;
  completely_control_time = false;
  pthread_mutex_unlock(&time_lock);
}

void cwtest_completely_control_time(bool start_of_epoch)
{
  if (!real_clock_gettime)
  {
    real_clock_gettime = (int (*)(clockid_t, struct timespec *))(intptr_t)dlsym(RTLD_NEXT, "clock_gettime");
  }

  if (!real_time)
  {
    real_time = (time_t (*)(time_t*))(intptr_t)dlsym(RTLD_NEXT, "time");
  }

  pthread_mutex_lock(&time_lock);
  completely_control_time = true;

  // Store the time at which the test scripts took control.  Do this for both
  // clock_gettime() and time().
  struct timespec ts;
  for (unsigned int i = 0;
       i < (sizeof(supported_clock_ids) / sizeof(supported_clock_ids[0]));
       ++i)
  {
    clockid_t clock_id = supported_clock_ids[i];
    if (start_of_epoch)
    {
      abs_timespecs[clock_id] = {0, 0};
    }
    else
    {
      real_clock_gettime(clock_id, &ts);
      abs_timespecs[clock_id] = ts;
    }
  }

  abs_time = real_time(NULL);

  pthread_mutex_unlock(&time_lock);
}

/// Lookup helper.  If there is a mapping of this host in host_mapping
/// apply it, otherwise just return the host requested.
static inline std::string host_lookup(const char* node)
{
  std::string host(node);
  std::map<std::string,std::string>::iterator iter = host_map.find(host);
  if (iter != host_map.end())
  {
    // We have a mapping which says "turn a lookup for node into a
    // lookup for iter->second".  Apply it.
    host = iter->second;
  }

  return host;
}

/// Replacement getaddrinfo.
int getaddrinfo(const char *node,
                const char *service,
                const struct addrinfo *hints,
                struct addrinfo **res)
{
  if (!real_getaddrinfo)
  {
    real_getaddrinfo = (int(*)(const char*, const char*, const struct addrinfo*, struct addrinfo**))(intptr_t)dlsym(RTLD_NEXT, "getaddrinfo");
  }

  return real_getaddrinfo(host_lookup(node).c_str(), service, (const struct addrinfo*)hints, (struct addrinfo**)res);
}

/// Replacement gethostbyname.
struct hostent* gethostbyname(const char *name)
{
  if (!real_gethostbyname)
  {
    real_gethostbyname = (struct hostent*(*)(const char*))(intptr_t)dlsym(RTLD_NEXT, "gethostbyname");
  }

  return real_gethostbyname(host_lookup(name).c_str());
}

/// Replacement clock_gettime.
int clock_gettime(clockid_t clk_id, struct timespec *tp) throw ()
{
  int rc;

  if (!real_clock_gettime)
  {
    real_clock_gettime = (int (*)(clockid_t, struct timespec *))(intptr_t)dlsym(RTLD_NEXT, "clock_gettime");
  }

  pthread_mutex_lock(&time_lock);

  if (completely_control_time)
  {
    if (abs_timespecs.find(clk_id) != abs_timespecs.end())
    {
      *tp = abs_timespecs[clk_id];
      rc = 0;
    }
    else
    {
      // We don't support this clock ID. Print a warning, and act like an
      // invalid clock ID has been requested.
      fprintf(stderr, "WARNING: Clock ID %d is not supported (%s:%d)\n",
              clk_id, __FILE__, __LINE__);
      rc = -1;
      errno = EINVAL;
    }
  }
  else
  {
    rc = real_clock_gettime(clk_id, tp);
  }

  if (!rc)
  {
    ts_add(*tp, time_offset, *tp);
  }

  pthread_mutex_unlock(&time_lock);

  return rc;
}


/// Replacement time().
time_t time(time_t* v) throw ()
{
  time_t rt;

  if (!real_time)
  {
    real_time = (time_t (*)(time_t*))(intptr_t)dlsym(RTLD_NEXT, "time");
  }

  pthread_mutex_lock(&time_lock);

  if (completely_control_time)
  {
    rt = abs_time;
  }
  else
  {
    // Get the real time in seconds since the epoch.
    rt = real_time(NULL);
  }

  // Add the seconds portion of the time offset.
  rt += time_offset.tv_sec;
  pthread_mutex_unlock(&time_lock);

  if (v != NULL)
  {
    // Pointer supplied, so set it to the returned value.
    *v = rt;
  }

  return rt;
}

/// Replacement pthread_cond_timedwait()
///
/// WARNING: This replacement calls through to the 2.3.2 version of the 
/// real pthread_cond_timedwait, regardless of the version the calling code
/// assumed it would be calling.  This will need updating to support other
/// libc versions.
///
/// WARNING THE SECOND: This assumes that the condition variable was created
/// with a condattr that specifies CLOCK_MONOTONIC.
///
/// http://blog.fesnel.com/blog/2009/08/25/preloading-with-multiple-symbol-versions/
int pthread_cond_timedwait(pthread_cond_t* cond,
                           pthread_mutex_t* mutex,
                           const struct timespec* abstime)
{
  struct timespec fixed_time;

  if (!real_pthread_cond_timedwait)
  {
    real_pthread_cond_timedwait = (pthread_cond_timedwait_func_t)(intptr_t)dlvsym(RTLD_NEXT, "pthread_cond_timedwait", "GLIBC_2.3.2");
    if (!real_pthread_cond_timedwait)
    {
      real_pthread_cond_timedwait = (pthread_cond_timedwait_func_t)(intptr_t)dlvsym(RTLD_NEXT, "pthread_cond_timedwait", "GLIBC_2.4.0");
    }
  }

  // Subtract our fake time and add the real time, this means the
  // relative delay is correct while allowing the calling code to think
  // it's woking with absolute time.
  //
  // Note we call the fake clock_gettime first, meaning that real_clock_gettime
  // will be set before the call to it in the next line.
  struct timespec fake_time;
  struct timespec real_time;
  struct timespec delta_time;
  clock_gettime(CLOCK_MONOTONIC, &fake_time);
  real_clock_gettime(CLOCK_MONOTONIC, &real_time);
  ts_sub(*abstime, fake_time, delta_time);
  ts_add(real_time, delta_time, fixed_time);

  return real_pthread_cond_timedwait(cond, mutex, &fixed_time);
}


void cwtest_control_fopen(FILE* fd)
{
  control_fopen = true;
  fopen_file_pointer = fd;
}


void cwtest_release_fopen()
{
  control_fopen = false;
  fopen_file_pointer = NULL;
}


FILE* fopen(const char *path, const char *mode)
{
  if (!real_fopen)
  {
    real_fopen = (FILE* (*)(const char*, const char*))dlsym(RTLD_NEXT, "fopen");
  }

  FILE* rc;

  if (control_fopen)
  {
    rc = fopen_file_pointer;
  }
  else
  {
    rc = real_fopen(path, mode);
  }

  return rc;
}
