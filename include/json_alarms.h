/**
 * @file json_alarms.h  Handler for UNIX signals.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2015  Metaswitch Networks Ltd
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
#ifndef JSON_ALARMS_H
#define JSON_ALARMS_H

#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <sys/stat.h>

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include "json_parse_utils.h"
#include "alarmdefinition.h"

namespace JSONAlarms
{
  // Parses alarms from a JSON file. This validates the alarms and populates
  // two alarm structures
  //
  // @params path - Location of the JSON file
  // @params result - Error string. Only set if an error was hit. This is 
  //                  returned so that different callers of this method can
  //                  log it appropriately
  // @params alarms - Vector of alarm definitions built from the JSON
  // @params header - Map of the alarm name to index
  // @returns - if the parsing was successful
  bool validate_alarms_from_json(std::string path,
                                 std::string& error,
                                 std::vector<AlarmDef::AlarmDefinition>& alarms,
                                 std::map<std::string, int>& header);

  // Wrapper functions for different callers of the JSON validation 
  // function
  bool validate_alarms_from_json(std::string path,
                                 std::string& error,
                                 std::map<std::string, int>& header);

  bool validate_alarms_from_json(std::string path,
                                 std::string& error,
                                 std::vector<AlarmDef::AlarmDefinition>& alarms);

  // Processes the name of an alarm into a human readable format, for example
  // "SPROUT_PROCESS_FAILURE" becomes "Sprout process failure"
  std::string process_alarm_name(std::string raw_name);

  // Prepares a string that can be used to report the error that a specific
  // field has exceeded the character limit
  std::string exceeded_character_limit_error(std::string field, int max_length, int index);

  // Writes a header file that includes the alarm IDs and their index
  void write_header_file(std::string name, std::map<std::string, int> alarms);
};

#endif
