/**
 * @file json_alarms.h  Handler for UNIX signals.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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

  // Prepares a string that can be used to report the error that a specific
  // field has exceeded the character limit
  std::string exceeded_character_limit_error(std::string field, int max_length, int index);

  // Writes a header file that includes the alarm IDs and their index
  void write_header_file(std::string name, std::map<std::string, int> alarms);
};

#endif
