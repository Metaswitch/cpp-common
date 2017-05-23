/**
 * @file accesslogger.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <stdio.h>

#include "accesslogger.h"

AccessLogger::AccessLogger(const std::string& directory)
{
  _logger = new Logger(directory, std::string("access"));
  _logger->set_flags(Logger::ADD_TIMESTAMPS|Logger::FLUSH_ON_WRITE);
}

AccessLogger::~AccessLogger()
{
  delete _logger;
}

void AccessLogger::log(const std::string& uri,
                       const std::string& method,
                       int rc,
                       unsigned long latency_us)
{
  char buf[BUFFER_SIZE];
  snprintf(buf, sizeof(buf),
           "%d %s %s %ld.%6.6ld seconds\n",
           rc,
           method.c_str(),
           uri.c_str(),
           latency_us / 1000000,
           latency_us % 1000000);
  _logger->write(buf);
}
