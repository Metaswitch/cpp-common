/**
 * @file health_checker.cpp
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


#include <cassert>
#include "health_checker.h"

#include "log.h"

void HealthChecker::hit_exception()
{
  _hit_exception = true;
}

void HealthChecker::health_check_passed()
{
  _recent_passes++;
  LOG_DEBUG("Health check passed, %d passes in last 60s", _recent_passes.load());
}

// Checking function which should be run every 60s on a thread. Aborts
// the process if we have hit an exception and health_check_passed()
// has not been called since this was last run.
void HealthChecker::do_check()
{
  if (_hit_exception.load() && (_recent_passes.load() == 0))
  {
    // LCOV_EXCL_START - only covered in "death tests"
    LOG_ERROR("Check for overall system health failed - aborting");
    assert(!"Health check failed");
    // LCOV_EXCL_STOP
  }
  else
  {
    LOG_DEBUG("Check for overall system health passed - not aborting");
  }
  _recent_passes = 0;
}

// LCOV_EXCL_START
void* HealthChecker::main_thread_function(void* health_checker)
{
  while (true)
  {
    sleep(60);
    ((HealthChecker*)health_checker)->do_check();
  }
}
// LCOV_EXCL_STOP
