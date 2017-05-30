/**
 * @file saslogger.h Utility function to log out errors in
 * the SAS connection
 *
 * Copyright (C) Metaswitch Networks 2014
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef SAS_LOGGER_H__
#define SAS_LOGGER_H__

#include "sas.h"

void sas_write(SAS::log_level_t sas_level, const char *module, int line_number, const char *fmt, ...);

#endif
