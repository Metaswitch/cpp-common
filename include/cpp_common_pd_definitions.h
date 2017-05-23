/**
 * @file cpp-common_pd_definitions.h Defines instances of PDLog for cpp-common
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
