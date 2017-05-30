/**
 * @file saslogger.cpp Utility function to log out errors in
 * the SAS connection
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <cstdarg>

#include "log.h"
#include "saslogger.h"

// LCOV_EXCL_START

void sas_write(SAS::log_level_t sas_level, const char *module, int line_number, const char *fmt, ...)
{
  int level;
  va_list args;

  switch (sas_level) {
    case SAS::LOG_LEVEL_DEBUG:
      level = Log::DEBUG_LEVEL;
      break;
    case SAS::LOG_LEVEL_VERBOSE:
      level = Log::VERBOSE_LEVEL;
      break;
    case SAS::LOG_LEVEL_INFO:
      level = Log::INFO_LEVEL;
      break;
    case SAS::LOG_LEVEL_STATUS:
      level = Log::STATUS_LEVEL;
      break;
    case SAS::LOG_LEVEL_WARNING:
      level = Log::WARNING_LEVEL;
      break;
    case SAS::LOG_LEVEL_ERROR:
      level = Log::ERROR_LEVEL;
      break;
    default:
      TRC_ERROR("Unknown SAS log level %d, treating as error level", sas_level);
      level = Log::ERROR_LEVEL;
    }

  va_start(args, fmt);
  Log::_write(level, module, line_number, fmt, args);
  va_end(args);
}

// LCOV_EXCL_STOP
