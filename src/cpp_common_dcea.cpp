/**
 * @file accesslogger.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
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

#include <string>
#include "craft_dcea.h"


// CPP_COMMON syslog identities
/**********************************************************
/ log_id
/ severity
/ Description: (formatted)
/ Cause: 
/ Effect:
/ Action:
**********************************************************/
PDLog CL_DIAMETER_START
  (
   CL_CPP_COMMON_ID + 1,
   PDLOG_NOTICE,
   "Diameter stack is starting",
   "Diameter stack is beginning initialization",
   "Normal",
   "None"
   );
PDLog  CL_DIAMETER_INIT_CMPL
  (
   CL_CPP_COMMON_ID + 2,
   PDLOG_NOTICE,
   "Diameter stack initialization completed",
   "Diameter stack has completed initialization",
   "Normal",
   "None"
   );
PDLog4<const char*, int, const char*, const char*> CL_DIAMETER_ROUTE_ERR
  (
   CL_CPP_COMMON_ID + 3,
   PDLOG_ERR,
   "Diameter routing error: %s for message with Command-Code %d, Destination-Host %s and Destination-Realm %s",
   "No route was found for a Diameter message",
   "The Diameter message with the specified command code could not be routed to the destination host with the destination realm",
   "(1). Check the hss_hostname and hss_port in the /etc/clearwater/config file for correctness. (2). Check to see that there is a route to the hss database.  Check for IP connectiovity between the homestead host and the hss host using ping.  Wireshark the interface on homestead and the hss"
   );
PDLog1<const char*> CL_DIAMETER_CONN_ERR
  (
   CL_CPP_COMMON_ID + 4,
   PDLOG_ERR,
   "Failed to make a Diameter connection to host %s",
   "A Diameter connection attempt failed to the specified host",
   "This impacts the ability to register, subscribe, or make a call",
   "(1). Check the hss_hostname and hss_port in the /etc/clearwater/config file for correctness.  (2). Check to see that there is a route to the hss database.  Check for IP connectiovity between the homestead host and the hss host using ping.  Wireshark the interface on homestead and the hss"
   );
PDLog4<const char*, const char*, const char*, int> CL_HTTP_COMM_ERR
  (
   CL_CPP_COMMON_ID + 5,
   PDLOG_ERR,
   "%s failed to communicate with http server %s with curl error %s code %d",
   "An HTTP connection attempt failed to the specified server with the specified error code",
   "This condition impacts the ability to register, subscribe, or make a call.",
   "(1). Check to see if the specified host has failed.  (2). Check to see if there is TCP connectivity to the host by using ping and/or Wireshark."
   );
