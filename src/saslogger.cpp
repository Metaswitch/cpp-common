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

void sas_write(sasclient_log_level_t sas_level,
               int32_t log_id_len,
               unsigned char* log_id,
               int32_t sas_ip_len,
               unsigned char* sas_ip,
               int32_t msg_len,
               unsigned char* msg)
{
  int level;
  std::string sas_level_str;

  // Convert the sasclient_log_level_t to the common log level to determine if we need
  // to print the log.
  // Covert the sasclient_log_level_t to a char* to pass to write_sas_log() as the Log namespace
  // has no knowledge of SAS-Client specific types.
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

  if (level > Log::loggingLevel)
  {
    return;
  }

  Log::write_sas_log(sas_level_str.c_str(),
                     log_id_len,
                     log_id,
                     sas_ip_len,
                     sas_ip,
                     msg_len,
                     msg);
}

// LCOV_EXCL_STOP
