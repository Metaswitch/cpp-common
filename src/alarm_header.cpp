/**
 * @file alarms_header.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */
#include "json_alarms.h"

int main(int argc, char**argv)
{
  std::string json_file;
  std::string process_name;
  int c;

  opterr = 0;
  while ((c = getopt (argc, argv, "j:n:")) != -1)
  {
    switch (c)
      {
      case 'j':
        json_file = optarg;
        break;
      case 'n':
        process_name = optarg;
        break;
      default:
        abort ();
      }
  }

  // Parse the JSON alarms and generate a header file that has the alarm IDs
  std::string result;
  std::map<std::string, int> header;

  bool rc = JSONAlarms::validate_alarms_from_json(json_file, result, header);

  if (rc)
  {
    JSONAlarms::write_header_file(process_name, header);  
  }
  else
  { 
    fprintf(stderr, "Invalid JSON file. Error: %s", result.c_str());
  }
}
