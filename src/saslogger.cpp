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
#include <string>

#include "log.h"
#include "saslogger.h"

// LCOV_EXCL_START

void sas_write(SAS::sas_log_level_t sas_level,
               int32_t log_id_len,
               unsigned char* log_id,
               int32_t sas_ip_len,
               unsigned char* sas_ip,
               int32_t msg_len,
               unsigned char* msg)
{
  int level;
  std::string sas_level_str;

  // Write all of the args to msg, then pass to

  // Convert the sasclient_log_level_t to the common log level to determine if we need
  // to print the log.
  // Covert the sasclient_log_level_t to a string so we can easily write it to the logline array.
  switch (sas_level) {
    case SASCLIENT_LOG_CRITICAL:
      level = Log::ERROR_LEVEL;
      sas_level_str = "Critical";
      break;
    case SASCLIENT_LOG_ERROR:
      level = Log::ERROR_LEVEL;
      sas_level_str = "Error";
      break;
    case SASCLIENT_LOG_WARNING:
      level = Log::WARNING_LEVEL;
      sas_level_str = "Warning";
      break;
    case SASCLIENT_LOG_INFO:
      level = Log::STATUS_LEVEL;
      sas_level_str = "Info";
      break;
    case SASCLIENT_LOG_DEBUG:
      level = Log::DEBUG_LEVEL;
      sas_level_str = "Debug";
      break;
    case SASCLIENT_LOG_TRACE:
      level = Log::DEBUG_LEVEL;
      sas_level_str = "Trace";
      break;
    case SASCLIENT_LOG_STATS:
      level = Log::INFO_LEVEL;
      sas_level_str = "Info";
      break;
    default:
      TRC_ERROR("Unknown SAS log level %d, treating as error level", sas_level);
      level = Log::ERROR_LEVEL;
      sas_level_str = "Error";
    }

  // Check if we actually need to print the recieved log.
  if (level > Log::loggingLevel)
  {
    return;
  }

  // Array comfortably larger than any expected log. Log::_write outputs a warning
  // if the resulting log to be logged is truncated due to being too long.
  int array_size = 10000;
  char logline[array_size]

 snsprintf(logline, array_size, "%s, ", sas_level_str);

  if (log_id != NULL)
  {
   snprintf(logline + strlen(logline), array_size, "%.*s,", log_id_len, log_id);
  }

  if (sas_ip != NULL)
  {
   snprintf(logline + strlen(logline), array_size, "%.*s,", sas_ip_len, sas_ip);
  }

 snprintf(logline + strlen(logline), array_size, "%.*s", msg_len, msg);

  // Set level to 0, so that the log level check in Log::write passes. Pass in an empty va_list
  // to Log::_write, as we don't need to format our msg any further.
  level = 0;
  va_list empty_va_list;
  Log::_write(level,
              NULL,
              0,
              logline,
              empty_va_list)
}

// LCOV_EXCL_STOP
