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
#include <boost/algorithm/string.hpp> 

// To add a new alarm:
//
//   - Add it to the JSON alarm file in the relevant repository.
//   
//   - If it's a new repo, then make sure that the alarm file gets 
//     installed to /usr/share/clearwater/infrastructure/alarms. 
namespace AlarmDef {

  // Sprout alarms: 1000->1499
  // Homestead alarms: 1500->1999
  // Ralf alarms: 2000->2499
  // Bono alarms: 2500->2999
  // Chronos alarms: 3000->3499
  // Cassandra alarms: 4000->4499
  // Memento alarms: 5000->5499
  // Astaire alarms: 5500->5999
  // Etcd alarms: 6500->6999
  // Alarms 7000->7999 are reserved
  // Cluster-manager alarms: 8000->8499
  // Config-manager alarms: 8500->8999
  // Queue-manager alarms: 9000->9499
  // Alarms 9500->9999 are reserved
  // Alarms 10000->10499 are reserved
  // Alarms 10500-11000 are reserved

  enum Severity {
    UNDEFINED_SEVERITY,
    CLEARED,
    INDETERMINATE,
    CRITICAL,
    MAJOR,
    MINOR,
    WARNING
  };

  inline bool operator > (Severity lhs, Severity rhs)
  {
    // Order the severities so we can do a simple comparison to determine any
    // severity change.
    unsigned int ordered_severities[] = {0, 1, 2, 6, 5, 4, 3};
    return ordered_severities[lhs] > ordered_severities[rhs];
  }

  inline bool operator < (Severity lhs, Severity rhs)
  {
    return !(lhs > rhs);
  }

  enum Cause {
    UNDEFINED_CAUSE,
    DATABASE_INCONSISTENCY = 160,
    SOFTWARE_ERROR = 163,
    UNDERLYING_RESOURCE_UNAVAILABLE = 554
  };

  struct SeverityDetails {
    SeverityDetails() {};

    SeverityDetails(Severity severity,
                    std::string description,
                    std::string details,
                    std::string cause,
                    std::string effect,
                    std::string action,
                    std::string extended_details,
                    std::string extended_description):
      _severity(severity),
      _description(description),
      _details(details),
      _cause(cause),
      _effect(effect),
      _action(action),
      _extended_details(extended_details),
      _extended_description(extended_description) {};

    Severity _severity;
    std::string _description;
    std::string _details;
    std::string _cause;
    std::string _effect;
    std::string _action;
    std::string _extended_details;
    std::string _extended_description;
  };

  struct AlarmDefinition {
    AlarmDefinition() {};

    AlarmDefinition(std::string name,
                    int index,
                    Cause cause,
                    std::vector<SeverityDetails> severity_details):
      _name(name),
      _index(index),
      _cause(cause),
      _severity_details(severity_details) {};

    std::string _name;
    int _index;
    Cause _cause;
    std::vector<SeverityDetails> _severity_details;
  };

  Cause cause_to_enum(std::string cause);
  Severity severity_to_enum(std::string severity);

  extern const std::vector<AlarmDefinition> alarm_definitions;
}

#endif

