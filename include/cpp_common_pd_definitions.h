/**
 * @file cpp-common_pd_definitions.h Defines instances of PDLog for cpp-common
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


#ifndef CPP_COMMON_PD_DEFINITIONS_H__
#define CPP_COMMON_PD_DEFINITIONS_H__

#include "pdlog.h"

// Defines instances of PDLog for the cpp-common module

// The fields for each PDLog instance contains:
//   Identity - Identifies the log id to be used in the syslog id field.
//   Severity - One of Emergency, Alert, Critical, Error, Warning, Notice,
//              and Info.  Only LOG_ERROR or LOG_NOTICE are used.
//   Message  - Formatted description of the condition.
//   Cause    - The cause of the condition.
//   Effect   - The effect the condition.
//   Action   - A list of one or more actions to take to resolve the condition
//              if it is an error.
static const PDLog CL_DIAMETER_START
(
  PDLogBase::CL_CPP_COMMON_ID + 1,
  LOG_NOTICE,
  "Diameter stack is starting.",
  "Diameter stack is beginning initialization.",
  "Normal.",
  "None."
);

static const PDLog CL_DIAMETER_INIT_CMPL
(
  PDLogBase::CL_CPP_COMMON_ID + 2,
  LOG_NOTICE,
  "Diameter stack initialization completed.",
  "Diameter stack has completed initialization.",
  "Normal.",
  "None."
);

static const PDLog2<int, const char*> CL_MEMCACHED_CLUSTER_UPDATE_STABLE
(
  PDLogBase::CL_CPP_COMMON_ID + 6,
  LOG_NOTICE,
  "The memcached cluster configuration has been updated. There are now %d nodes in the cluster.",
  "A change has been detected to the %s configuration file that has changed the memcached cluster.",
  "Normal.",
  "None."
);

static const PDLog3<int, int, const char*> CL_MEMCACHED_CLUSTER_UPDATE_RESIZE
(
  PDLogBase::CL_CPP_COMMON_ID + 7,
  LOG_NOTICE,
  "The memcached cluster configuration has been updated. The cluster is resizing from %d nodes to %d nodes.",
  "A change has been detected to the %s configuration file that has changed the memcached cluster.",
  "Normal.",
  "None."
);

static const PDLog2<const char*, const char*> CL_CM_CONNECTION_PARTIAL_ERROR
(
  PDLogBase::CL_CPP_COMMON_ID + 8,
  LOG_INFO,
  "Some connections between %s and %s applications have failed.",
  "This process was unable to contact at least one instance of the application "
  "it's trying to connect to, but did make some successful contact",
  "This process was unable to contact at least one instance of the application "
  "it's trying to connect to",
  "(1). Check that the application this process is trying to connect to is running."
  "(2). Check the configuration in /etc/clearwater is correct."
  "(3). Check that this process has connectivity to the application it's trying to connect to."
);

static const PDLog2<const char*, const char*> CL_CM_CONNECTION_ERRORED
(
  PDLogBase::CL_CPP_COMMON_ID + 9,
  LOG_ERR,
  "%s is unable to contact any %s applications. It will periodically "
  "attempt to reconnect",
  "This process is unable to contact any instances of the application "
  "it's trying to connect to",
  "This process is unable to contact any instances of the application "
  "it's trying to connect to",
  "(1). Check that the application this process is trying to connect to is running."
  "(2). Check the configuration in /etc/clearwater is correct."
  "(3). Check that this process has connectivity to the application it's trying to connect to."
);

static const PDLog2<const char*, const char*> CL_CM_CONNECTION_CLEARED
(
  PDLogBase::CL_CPP_COMMON_ID + 10,
  LOG_INFO,
  "Connection between %s and %s has been restored.",
  "This process can now contact at least one instance of the application it's "
  "trying to connect to, and has seen no errors in the previous monitoring period",
  "Normal.",
  "None."
);

static const PDLog CL_DNS_FILE_MALFORMED
(
  PDLogBase::CL_CPP_COMMON_ID + 11,
  LOG_ERR,
  "DNS config file is malformed.",
  "The DNS config file /etc/clearwater/dns.json is invalid JSON.",
  "The DNS config file will be ignored, and all DNS queries will be directed at "
  "the DNS server rather than using any local overrides.",
  "(1). Check the DNS config file for correctness."
  "(2). Upload the corrected config with "
  "/usr/share/clearwater/clearwater-config-manager/scripts/upload_dns_json"
);

static const PDLog CL_DNS_FILE_DUPLICATES
(
  PDLogBase::CL_CPP_COMMON_ID + 12,
  LOG_INFO,
  "Duplicate entries found in the DNS config file",
  "The DNS config file /etc/clearwater/dns.json contains duplicate entries.",
  "Only the first of the duplicates will be used - the others will be ignored.",
  "(1). Check the DNS config file for duplicates."
  "(2). Upload the corrected config with "
  "/usr/share/clearwater/clearwater-config-manager/scripts/upload_dns_json"
);

static const PDLog CL_DNS_FILE_MISSING
(
  PDLogBase::CL_CPP_COMMON_ID + 13,
  LOG_ERR,
  "DNS config file is missing.",
  "The DNS config file /etc/clearwater/dns.json is not present.",
  "The DNS config file will be ignored, and all DNS queries will be directed at "
  "the DNS server rather than using any local overrides.",
  "(1). Replace the missing DNS config file if desired."
  "(2). Upload the corrected config with "
  "/usr/share/clearwater/clearwater-config-manager/scripts/upload_dns_json "
  "(if no config file is present, no DNS overrides will be applied)"
);

static const PDLog CL_DNS_FILE_BAD_ENTRY
(
  PDLogBase::CL_CPP_COMMON_ID + 14,
  LOG_ERR,
  "DNS config file has a malformed entry.",
  "The DNS config file /etc/clearwater/dns.json contains a malformed entry.",
  "The malformed entry will be ignored. Other, correctly formed, entries will "
  "still be used.",
  "(1). Check the DNS config file for correctness."
  "(2). Upload the corrected config with "
  "/usr/share/clearwater/clearwater-config-manager/scripts/upload_dns_json"
);

#endif
