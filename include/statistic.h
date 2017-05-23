/**
 * @file statistic.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef STATISTICS_H__
#define STATISTICS_H__

#include <string>
#include <zmq.h>
#include <vector>

#include <pthread.h>

#include "eventq.h"
#include "zmq_lvc.h"

class Statistic
{
public:
  Statistic(std::string statname, LastValueCache* lvc);
  ~Statistic();

  void report_change(std::vector<std::string> new_value);

  static int known_stats_count();
  static std::string *known_stats();

  // Thread entry point for reporting thread.
  static void* reporter_thread(void* p);

private:
  void reporter();

  std::string _statname;
  void *_publisher;
  pthread_t _reporter;
  eventq<std::vector<std::string> > _stat_q;  // input queue

  static const int MAX_Q_DEPTH = 100;
};

#endif
