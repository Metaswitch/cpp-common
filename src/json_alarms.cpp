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

namespace JSONAlarms
{
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

          JSON_GET_STRING_MEMBER(*alarms_def_it, "severity", severity);
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
            char error_text[100];
            sprintf(error_text, "alarm %d: 'details' exceeds %d char limit", index, 255);
            error = std::string(error_text);
            return false;
          }

          JSON_GET_STRING_MEMBER(*alarms_def_it, "description", description);
          if (description.length() > 255)
          {
            char error_text[100];
            sprintf(error_text, "alarm %d: 'description' exceeds %d char limit", index, 255);
            error = std::string(error_text);
            return false;
          }

          AlarmDef::SeverityDetails sd = {e_severity, description, details}; 
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
          AlarmDef::AlarmDefinition ad = {index,
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
