/**
 * @file alarmdefinition.h
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

#ifndef ALARM_DEFINITION_H__
#define ALARM_DEFINITION_H__

#include <string>
#include <vector>

// To add a new alarm:
//
//   - If the alarm is for a defined issuer with previous alarm defintions,
//     simply add a new Index enumeration for the issuer at the end of its
//     existing section. 
//
//   - If the alarm is for a new issuer. Add a new Issuer enumeration at 
//     the end of the existing list for the new issuer. Add a new section
//     at the bottom of the Index enumeration for the new issuer. The first
//     value in the section should be set to 500 more than the first value
//     in the previous section (i.e. room for 500 alarm index values per
//     issuer).
//
//   - For any new alarm Index enumerations defined, add a corresponding
//     AlarmDefinition to alarm_definitions in alarmdefinition.cpp.

namespace AlarmDef {

  enum Index {
    UNDEFINED_INDEX,

    SPROUT_PROCESS_FAIL = 1000,
    SPROUT_HOMESTEAD_COMM_ERROR,
    SPROUT_MEMCACHED_COMM_ERROR,
    SPROUT_REMOTE_MEMCACHED_COMM_ERROR,
    SPROUT_CHRONOS_COMM_ERROR,
    SPROUT_RALF_COMM_ERROR,
    SPROUT_ENUM_COMM_ERROR,
    SPROUT_VBUCKET_ERROR,
    SPROUT_REMOTE_VBUCKET_ERROR,

    HOMESTEAD_PROCESS_FAIL = 1500,
    HOMESTEAD_CASSANDRA_COMM_ERROR,
    HOMESTEAD_HSS_COMM_ERROR,

    RALF_PROCESS_FAIL = 2000,
    RALF_MEMCACHED_COMM_ERROR,
    RALF_CHRONOS_COMM_ERROR,
    RALF_CDF_COMM_ERROR,
    RALF_VBUCKET_ERROR,

//  BONO_PROCESS_FAIL = 2500

    CHRONOS_PROCESS_FAIL = 3000,
    CHRONOS_TIMER_POP_ERROR,

    MEMCACHED_PROCESS_FAIL = 3500,
    
    CASSANDRA_PROCESS_FAIL = 4000,
    CASSANDRA_RING_NODE_FAIL,

    MONIT_PROCESS_FAIL = 4500
  };

  enum Severity {
    UNDEFINED_SEVERITY,
    CLEARED,
    INDETERMINATE,
    CRITICAL,
    MAJOR,
    MINOR,
    WARNING
  };

  enum Cause {
    UNDEFINED_CAUSE,
    SOFTWARE_ERROR = 163,
    UNDERLAYING_RESOURCE_UNAVAILABLE = 165
  };

  enum Issuer {
    UNDEFINED_ISSUER,
    SPROUT,
    HOMESTEAD,
    RALF,
    CHRONOS,
    MEMCACHED,
    CASSANDRA,
    MONIT
  };

  struct SeverityDetails {
    const Severity    _severity;
    const std::string _description;
    const std::string _details;
  };

  struct AlarmDefinition {
    const Index _index;
    const Cause _cause;
    const std::vector<SeverityDetails> _severity_details;
  };

  extern const std::vector<AlarmDefinition> alarm_definitions;
}

#endif

