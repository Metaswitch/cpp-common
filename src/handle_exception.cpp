/**
 * @file handle_exception.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2015  Metaswitch Networks Ltd
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

#include <pthread.h>
#include <setjmp.h>
#include <unistd.h>
#include <cstdlib>
#include <signal.h>

#include "handle_exception.h"

pthread_key_t _jmp_buf;

HandleException::HandleException(int ttl, 
                                 bool attempt_quiesce) :
  _ttl(ttl),
  _attempt_quiesce(attempt_quiesce)
{
}

HandleException::~HandleException()
{
}

void HandleException::handle_exception()
{
  // Check if there's a stored jmp_buf on the thread and handle if there is
  jmp_buf* env = (jmp_buf*)pthread_getspecific(_jmp_buf);

  if (env != NULL)
  {
    // Create a child thread, then abort it to create a core file.
    if (!fork())
    {
      // TODO Set thread local env to null
      abort();
    }

    // Let the health check know that an exception has occurred

    // Jump back to the stored state
    longjmp(*env, 1);
  }
}

void HandleException::delayed_exit_thread()
{
  pthread_create(&_delayed_exit_thread,
                 NULL,
                 delayed_exit_thread_func,
                 (void*)this);
  pthread_detach(_delayed_exit_thread);
}

void* HandleException::delayed_exit_thread_func(void* wt)
{
  // Choose a random time between now and 10 mins
  sleep(((HandleException*)wt)->_ttl);

  // Raise a SIGTERM/SIGQUIT
  if (((HandleException*)wt)->_attempt_quiesce)
  {
    raise(SIGQUIT);
    sleep(10);
  }

  raise(SIGTERM);

  return NULL;
}
