/**
 * @file syslog_facade.h
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014  Metaswitch Networks Ltd
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


#ifndef SYSLOG_FACADE_H__
#define SYSLOG_FACADE_H__

#include <features.h>
#define __need___va_list
#include <stdarg.h>

// Facade to avoid name collision between syslog.h and log.h
// Note that this is an extern "C" type file and is almost an exact duplicate of syslog.h

extern void closelog (void);
extern void openlog (__const char *__ident, int __option, int __facility);
extern void syslog (int __pri, __const char *__fmt, ...)
  __attribute__ ((__format__ (__printf__, 2, 3)));

#define PDLOG_PID         0x01    /* log the pid with each message */

#define PDLOG_EMERG       0       /* system is unusable */
#define PDLOG_ALERT       1       /* action must be taken immediately */
#define PDLOG_CRIT        2       /* critical conditions */
#define PDLOG_ERR         3       /* error conditions */
#define PDLOG_WARNING     4       /* warning conditions */
#define PDLOG_NOTICE      5       /* normal but significant condition */
#define PDLOG_INFO        6       /* informational */


#define PDLOG_LOCAL0      (16<<3) /* reserved for local use */
#define PDLOG_LOCAL1      (17<<3) /* reserved for local use */
#define PDLOG_LOCAL2      (18<<3) /* reserved for local use */
#define PDLOG_LOCAL3      (19<<3) /* reserved for local use */
#define PDLOG_LOCAL4      (20<<3) /* reserved for local use */
#define PDLOG_LOCAL5      (21<<3) /* reserved for local use */
#define PDLOG_LOCAL6      (22<<3) /* reserved for local use */


#endif
