/**
 * @file json_alarms.cpp  Handler for UNIX signals.
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

#include "json_alarms.h"
#include <algorithm>

namespace JSONAlarms
{
  // LCOV_EXCL_START - The other helper function is covered by UTs
  bool validate_alarms_from_json(std::string path,
                                 std::string& error,
                                 std::map<std::string, int>& header)
  {
    std::vector<AlarmDef::AlarmDefinition> unused;
    return validate_alarms_from_json(path, error, unused, header);
  }
  // LCOV_EXCL_STOP

  bool validate_alarms_from_json(std::string path,
                                 std::string& error,
                                 std::vector<AlarmDef::AlarmDefinition>& alarms)
  {
    std::map<std::string, int> unused;
    return validate_alarms_from_json(path, error, alarms, unused);
  }

  bool validate_alarms_from_json(std::string path,
                                 std::string& error,
                                 std::vector<AlarmDef::AlarmDefinition>& alarms,
                                 std::map<std::string, int>& header)
  {
    // Read from the file
    std::ifstream fs(path.c_str());
    std::string alarms_str((std::istreambuf_iterator<char>(fs)),
                            std::istreambuf_iterator<char>());

    if (alarms_str == "")
    {
      // LCOV_EXCL_START - Not tested in UT
      error = "Empty/unopenable file";
      return false;
      // LCOV_EXCL_STOP
    }

    // Now parse the document
    rapidjson::Document doc;
    doc.Parse<0>(alarms_str.c_str());

    if (doc.HasParseError())
    {
      error = std::string("Invalid JSON file. Error: ").
                append(rapidjson::GetParseError_En(doc.GetParseError()));
      return false;
    }

    try
    {
      // Parse the JSON file. Each alarm should:
      // - have a cleared alarm and a non-cleared alarm
      // - have a cause that matches an allowed cause
      // - have a severity that matches an allowed severity
      // - have the description/details text be less than 256 characters. 
      // - have a more detailed cause text.
      // - have an effect text.
      // - have an action text.

      JSON_ASSERT_CONTAINS(doc, "alarms");
      JSON_ASSERT_ARRAY(doc["alarms"]);
      const rapidjson::Value& alarms_arr = doc["alarms"];

      for (rapidjson::Value::ConstValueIterator alarms_it = alarms_arr.Begin();
           alarms_it != alarms_arr.End();
           ++alarms_it)
      {
        int index;
        std::string cause;
        std::string name;

        JSON_GET_INT_MEMBER(*alarms_it, "index", index);
        JSON_GET_STRING_MEMBER(*alarms_it, "cause", cause);
        AlarmDef::Cause e_cause = AlarmDef::cause_to_enum(cause);
        if (e_cause == AlarmDef::UNDEFINED_CAUSE)
        {
          char error_text[100];
          sprintf(error_text, "alarm %d: Invalid cause %s", index, cause.c_str());
          error = std::string(error_text);
          return false;
        }

        JSON_GET_STRING_MEMBER(*alarms_it, "name", name);
        header[name] = index;

        JSON_ASSERT_CONTAINS(*alarms_it, "levels");
        JSON_ASSERT_ARRAY((*alarms_it)["levels"]);
        const rapidjson::Value& alarms_def_arr = (*alarms_it)["levels"];

        std::vector<AlarmDef::SeverityDetails> severity_vec;
        bool found_cleared = false;
        bool found_non_cleared = false;

        for (rapidjson::Value::ConstValueIterator alarms_def_it = alarms_def_arr.Begin();
             alarms_def_it != alarms_def_arr.End();
             ++alarms_def_it)
        {
          std::string severity;
          std::string details;
          std::string description;
          std::string detailed_cause;
          std::string effect;
          std::string action;
          std::string extended_details;
          std::string extended_description;

          JSON_GET_STRING_MEMBER(*alarms_def_it, "severity", severity);
          // Alarms are stored in ITU Alarm Table using
          // ituAlarmPerceivedSeverity as below.
          // Alarm Model Table stores alarms using alarmModelState. The
          // mapping between state and severity is described in RFC 3877
          // section 5.4: https://tools.ietf.org/html/rfc3877#section-5.4
          // The function AlarmTableDef::state() maps severities to states.
          AlarmDef::Severity e_severity = AlarmDef::severity_to_enum(severity);
          if (e_severity == AlarmDef::UNDEFINED_SEVERITY)
          {
            char error_text[100];
            sprintf(error_text, "alarm %d: Invalid severity %s", index, severity.c_str());
            error = std::string(error_text);
            return false;
          }
          else if (e_severity == AlarmDef::CLEARED)
          {
            found_cleared = true;
          }
          else
          {
            found_non_cleared = true;
          }

          JSON_GET_STRING_MEMBER(*alarms_def_it, "details", details);
          if (details.length() > 255)
          {
            error = exceeded_character_limit_error("details", 255, index);
            return false;
          }

          // We check if any extended details have been included in each
          // alarms's JSON file.
          if (alarms_def_it->HasMember("extended_details"))
          {
            JSON_GET_STRING_MEMBER(*alarms_def_it, "extended_details", extended_details);
            if (extended_details.length() > 4096)
            {
              error = exceeded_character_limit_error("extended_details", 4096, index);
              return false;
            }
          }
          else
          {
            // If there are no extended details we fill in the extended
            // details attribute with the regular details field.
            extended_details = details;
          }

          JSON_GET_STRING_MEMBER(*alarms_def_it, "description", description);
          if (description.length() > 255)
          {
            error = exceeded_character_limit_error("description", 255, index);
            return false;
          }
          
          // We check if an extended description has been included in each
          // alarms's JSON file.
          if (alarms_def_it->HasMember("extended_description"))
          {
            JSON_GET_STRING_MEMBER(*alarms_def_it, "extended_description", extended_description);
            if (extended_description.length() > 4096)
            {
              error = exceeded_character_limit_error("extended_description", 4096, index);
              return false;
            }
          }
          else
          {
            // If there is no extended description we fill in the extended
            // description attribute with the regular description field.
            extended_description = description;
          }

          JSON_GET_STRING_MEMBER(*alarms_def_it, "cause", detailed_cause);
          if (detailed_cause.length() > 4096)
          {
            error = exceeded_character_limit_error("cause", 4096, index);
            return false;
          }

          JSON_GET_STRING_MEMBER(*alarms_def_it, "effect", effect);
          if (effect.length() > 4096)
          {
            error = exceeded_character_limit_error("effect", 4096, index);
            return false;
          }

          JSON_GET_STRING_MEMBER(*alarms_def_it, "action", action);
          if (action.length() > 4096)
          {
            error = exceeded_character_limit_error("action", 4096, index);
            return false;
          }

          AlarmDef::SeverityDetails sd(e_severity,
                                       description,
                                       details,
                                       detailed_cause,
                                       effect,
                                       action,
                                       extended_details,
                                       extended_description);
          severity_vec.push_back(sd);
        }
      
        if (!found_cleared)
        {
          char error_text[100];
          sprintf(error_text, "alarm %d.*: must define a CLEARED severity", index);
          error = std::string(error_text);
          return false;
        }
        else if (!found_non_cleared)
        {
          char error_text[100];
          sprintf(error_text, "alarm %d.*: must define at least one non-CLEARED severity", index);
          error = std::string(error_text);
          return false;
        }
        else 
        {
          // Here we use the human readable form of the alarm's name
          AlarmDef::AlarmDefinition ad = {process_alarm_name(name),
                                          index,
                                          e_cause,
                                          severity_vec};
          alarms.push_back(ad);
        }
      }
    }
    catch (JsonFormatError err)
    {
      error = std::string("Invalid JSON file: ").append(err._file).
                                                 append(", line: ").
                                                 append(std::to_string(err._line));
      return false;
    }

    return true;
  }

  // Function to transform the name of each alarm from e.g.
  // "SPROUT_PROCESS_FAILURE" to a human readable format e.g. "Sprout process
  // failure"
  std::string process_alarm_name(std::string name)
  {
    std::replace(name.begin(), name.end(), '_', ' ');
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    name[0] = toupper(name[0]);
    return name;
  }

  std::string exceeded_character_limit_error(std::string field, int max_length, int index)
  {
    char error_text[100];
    sprintf(error_text, "alarm %d: '%s' exceeds %d char limit", index, field.c_str(), max_length);
    return std::string(error_text);
  }

  // LCOV_EXCL_START - This function isn't tested in UTs
  void write_header_file(std::string name, std::map<std::string, int> alarms)
  {
    std::string alarm_values;

    for(std::map<std::string, int>::const_iterator key_it = alarms.begin();
        key_it != alarms.end();
        ++key_it)
    {
      alarm_values.append("static const int ").
                   append(key_it->first).
                   append(" = ").
                   append(std::to_string(key_it->second)).
                   append(";\n");
    }

    std::string alarms_header = "#ifndef " + name + "_alarm_definition_h\n" \
                                "#define " + name + "_alarm_definition_h\n" \
                                "namespace AlarmDef {\n" \
                                + alarm_values + \
                                "}\n#endif";

    std::string filename = name + "_alarmdefinition.h";
    
    std::ofstream file(filename);
    file << alarms_header;
  }
  // LCOV_EXCL_STOP
};
