/**
 * @file health_checker.h
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2015 Metaswitch Networks Ltd
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

#ifndef HEALTH_CHECKER_H
#define HEALTH_CHECKER_H

#include <atomic>
#include <pthread.h>

// Health-checking object which:
//  - is notified when "healthy behaviour" happens (e.g. a 200 OK response)
//  - is notified when an exception is hit
//  - checks every 60 seconds to see if an exception has been hit and
//    no healthy behaviour has been seen since the last check, and
//    aborts the process if so.
class HealthChecker
{
public:
  HealthChecker();
  virtual ~HealthChecker();

  // Virtual for mocking in UT
  virtual void health_check_passed();
  void hit_exception();
  void do_check();
  void start_thread();
  void terminate();

  static void* static_main_thread_function(void* health_checker);
  void main_thread_function();
  
private:
  std::atomic_int _recent_passes;
  std::atomic_bool _hit_exception;
  std::atomic_bool _terminate;
  pthread_cond_t _condvar;
  pthread_mutex_t _condvar_lock;
  pthread_t _health_check_thread;
};

#endif
