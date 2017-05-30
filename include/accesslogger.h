/**
 * @file accesslogger.h Declaration of AccessLogger class.
 *
 * Copyright (C) Metaswitch Networks 2014
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef ACCESSLOGGER_H__
#define ACCESSLOGGER_H__

#include <sstream>

#include "logger.h"

class AccessLogger
{
public:
  AccessLogger(const std::string& directory);
  ~AccessLogger();

  void log(const std::string& url,
           const std::string& method,
           int rc,
           unsigned long latency_us);

private:
  static const int BUFFER_SIZE = 1000;

  Logger* _logger;
};

#endif

