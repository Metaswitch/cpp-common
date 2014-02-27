/**
 * @file statistic.cpp
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

#include "statistic.h"
#include "zmq_lvc.h"
#include "log.h"

#include <string>

Statistic::Statistic(std::string statname, LastValueCache* lvc) :
  _statname(statname),
  _publisher(NULL),
  _stat_q(MAX_Q_DEPTH)
{
  LOG_DEBUG("Creating %s statistic reporter", _statname.c_str());

  // Permit a NULL LVC as this is useful for fake objects in UTs.
  if (lvc != NULL)
  {
    _publisher = lvc->get_internal_publisher(statname);
  }

  // Spawn a thread to handle the statistic reporting
  int rc = pthread_create(&_reporter, NULL, &reporter_thread, (void*)this);

  if (rc < 0)
  {
    // LCOV_EXCL_START
    LOG_ERROR("Error creating statistic thread for %s", _statname.c_str());
    // LCOV_EXCL_STOP
  }
}


Statistic::~Statistic()
{
  // Signal the reporting thread.
  _stat_q.terminate();

  // Wait for the reporting thread to exit.
  pthread_join(_reporter, NULL);
}


/// Report the latest value of a statistic. Safe to be called by
/// multiple threads, no lock required.
void Statistic::report_change(std::vector<std::string> new_value)
{
  if (!_stat_q.push_noblock(new_value))
  {
    // LCOV_EXCL_START
    LOG_DEBUG("Statistic %s queue overflowed", _statname.c_str());
    // LCOV_EXCL_STOP
  }
}


void Statistic::reporter()
{
  LOG_DEBUG("Initializing inproc://%s statistic reporter", _statname.c_str());

  std::vector<std::string> new_value;

  while (_stat_q.pop(new_value))
  {
    if (_publisher != NULL)
    {
      LOG_DEBUG("Send new value for statistic %s, size %d",
                _statname.c_str(),
                new_value.size());
      std::string status = "OK";

      // If there's no message, just send the envelope and status line.
      if (new_value.empty())
      {
        zmq_send(_publisher, _statname.c_str(), _statname.length(), ZMQ_SNDMORE);
        zmq_send(_publisher, status.c_str(), status.length(), 0);
      }
      else
      {
        // Otherwise send the envelope, status line, and body, remembering to
        // set SNDMORE on all but the last section.
        zmq_send(_publisher, _statname.c_str(), _statname.length(), ZMQ_SNDMORE);
        zmq_send(_publisher, status.c_str(), status.length(), ZMQ_SNDMORE);
        std::vector<std::string>::iterator it;
        for (it = new_value.begin(); it + 1 != new_value.end(); ++it)
        {
          zmq_send(_publisher, it->c_str(), it->length(), ZMQ_SNDMORE); //LCOV_EXCL_LINE
        }
        zmq_send(_publisher, it->c_str(), it->length(), 0);
      }
    }
  }
}


void* Statistic::reporter_thread(void* p)
{
  ((Statistic*)p)->reporter();
  return NULL;
}


