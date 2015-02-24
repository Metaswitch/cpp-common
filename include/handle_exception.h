/**
 * @file handle_exception
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2015  Metaswitch Networks Ltd
 *
 * Parts of this module were derived from GPL licensed PJSIP sample code
 * with the following copyrights.
 *   Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 *   Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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

#ifndef _HANDLE_EXCEPTION_H__
#define _HANDLE_EXCEPTION_H__

#include <pthread.h>
#include <setjmp.h>

class HandleException
{
public:
  /// Constructor
  HandleException(int ttl, bool attempt_quiesce);

  /// Destructor
  ~HandleException();

  /// Handle an exception - call longjmp if there's a stored jmp_buf
  void handle_exception();

  /// Create a thread that kills the process after a random time
  void delayed_exit_thread();

private:
  /// Called by a new thread when an exception is hit. Kills the 
  /// process after a random time
  static void* delayed_exit_thread_func(void* det);

  /// The delayed exit thread
  pthread_t _delayed_exit_thread;

  /// The maximum time the process should live for
  int _ttl;

  /// Whether the exception handler should attempt to quiesce the process
  bool _attempt_quiesce;
};

/// Stored environment
extern pthread_key_t _jmp_buf;

/// TRY Macro
#define CW_TRY                                                                 \
jmp_buf env;                                                                   \
if (setjmp(env) == 0)                                                          \
{                                                                              \
  pthread_setspecific(_jmp_buf, env);

/// EXCEPT Macro
#define CW_EXCEPT(HANDLE_EXCEPTION)                                            \
}                                                                              \
else                                                                           \
{                                                                              \
  /* Spin off waiting thread */                                                \
  HANDLE_EXCEPTION->delayed_exit_thread(); 

/// END Macro
#define CW_END                                                                 \
  /* Tidy up thread local data */                                              \
  pthread_setspecific(_jmp_buf, NULL);                                         \
}

#endif
