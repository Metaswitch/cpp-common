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

void sas_write(sasclient_log_level_t level,
               int32_t log_id_len,
               unsigned char* log_id,
               int32_t sas_ip_len,
               unsigned char* sas_ip,
               int32_t msg_len,
               unsigned char* msg);
{
  int level;

  // Convert the sasclient_log_level_t to the common log level to determine if we need
  // to print the log.
  switch (sas_level) {
    case SAS::SASCLIENT_LOG_CRITICAL:
      level = Log::ERROR_LEVEL;
      break;
    case SAS::SASCLIENT_LOG_ERROR:
      level = Log::ERROR_LEVEL;
      break;
    case SAS::SASCLIENT_LOG_WARNING:
      level = Log::WARNING_LEVEL;
      break;
    case SAS::SASCLIENT_LOG_INFO:
      level = Log::INFO_LEVEL;
      break;
    case SAS::SASCLIENT_LOG_DEBUG:
      level = Log::DEBUG_LEVEL;
      break;
    case SAS::SASCLIENT_LOG_TRACE:
      level = Log::DEBUG_LEVEL;
      break;
    case SAS::SASCLIENT_LOG_STATS:
      level = Log::INFO_LEVEL;
      break;
    default:
      TRC_ERROR("Unknown SAS log level %d, treating as error level", sas_level);
      level = Log::ERROR_LEVEL;
    }

  if (level > Log::loggingLevel)
  {
    return;
  }

  pthread_mutex_lock(&Log::serialization_lock);
  if (!Log::logger)
  {
    // LCOV_EXCL_START
    pthread_mutex_unlock(&Log::serialization_lock);
    return;
    // LCOV_EXCL_STOP
  }

  pthread_cleanup_push(release_lock, 0);

  char logline[MAX_LOGLINE];

  int written = 0;
  int truncated = 0;

  pthread_t thread = pthread_self();

  if (!log_id == NULL)
  {
    written = snprintf(logline, MAX_LOGLINE - 2, "[%lx] %d,", thread, sas_level);
    written += snprintf(logline + strlen(logline), MAX_LOGLINE - 2, "%.*s,", log_id_len, log_id);
  }

  if (!sap_ip == NULL)
  {
    written += snprintf(logline + strlen(logline), MAX_LOGLINE - 2, "%.*s,", sas_ip_len, sas_ip);
  }

  written += snprintf(logline + strlen(logline), MAX_LOGLINE - 2, "%.*s\n", msg_len, msg);


  // snprintf and vsnprintf return the bytes that would have been
  // written if their second argument was large enough, so we need to
  // reduce the size of written to compensate if it is too large.
  written = std::min(written, MAX_LOGLINE - 2);

  if (written > (MAX_LOGLINE - 2))
  {
    truncated = written - (MAX_LOGLINE - 2);
    written = MAX_LOGLINE - 2;
  }

  // Add a new line and null termination.
  logline[written] = '\n';
  logline[written+1] = '\0';

  Log::logger->write(logline);
  if (truncated > 0)
  {
    char buf[128];
    snprintf(buf, 128, "Previous log was truncated by %d characters\n", truncated);
    Log::logger->write(buf);
  }
  pthread_cleanup_pop(0);
  pthread_mutex_unlock(&Log::serialization_lock);

}

// LCOV_EXCL_STOP
