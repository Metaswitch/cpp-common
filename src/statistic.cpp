/**
 * @file statistic.cpp
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
  TRC_DEBUG("Creating %s statistic reporter", _statname.c_str());

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
    TRC_ERROR("Error creating statistic thread for %s", _statname.c_str());
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
    TRC_DEBUG("Statistic %s queue overflowed", _statname.c_str());
    // LCOV_EXCL_STOP
  }
}


void Statistic::reporter()
{
  TRC_DEBUG("Initializing inproc://%s statistic reporter", _statname.c_str());

  std::vector<std::string> new_value;

  while (_stat_q.pop(new_value))
  {
    if (_publisher != NULL)
    {
      TRC_DEBUG("Send new value for statistic %s, size %d",
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


