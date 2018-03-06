/**
 * @file sasservice.cpp
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <sys/stat.h>
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "json_parse_utils.h"
#include <fstream>
#include <stdlib.h>

#include "sas.h"
#include "log.h"
#include "sasservice.h"

SasService::SasService(std::string configuration) :
  _configuration(configuration),
  _updater(NULL)
{
  extract_config();
}

void SasService::extract_config()
{
  // Check whether the file exists.
  struct stat s;
  TRC_DEBUG("stat(%s) returns %d", _configuration.c_str(), stat(_configuration.c_str(), &s));
  if ((stat(_configuration.c_str(), &s) != 0) &&
      (errno == ENOENT))
  {
    TRC_STATUS("No SAS configuration (file %s does not exist)",
               _configuration.c_str());
    return;
  }

  TRC_STATUS("Loading SAS configuration from %s", _configuration.c_str());

  // Read from the file
  std::ifstream fs(_configuration.c_str());
  std::string sas_str((std::istreambuf_iterator<char>(fs)),
                       std::istreambuf_iterator<char>());

  if (sas_str == "")
  {
    TRC_ERROR("Failed to read SAS configuration data from %s",
              _configuration.c_str());
    return;
  }

  // Now parse the document
  rapidjson::Document doc;
  doc.Parse<0>(sas_str.c_str());

  if (doc.HasParseError())
  {
    TRC_ERROR("Failed to read SAS configuration data: %s\nError: %s",
              sas_str.c_str(),
              rapidjson::GetParseError_En(doc.GetParseError()));
    return;
  }

  try
  {

    JSON_ASSERT_CONTAINS(doc, "sas_servers");
    JSON_ASSERT_ARRAY(doc["sas_servers"]);
    rapidjson::Value& sas_servers = doc["sas_servers"];

    for (rapidjson::Value::ValueIterator sas_it = sas_servers.Begin();
         sas_it != sas_servers.End();
         ++sas_it)
    {
      JSON_ASSERT_OBJECT(*sas_it);
      JSON_ASSERT_CONTAINS(*sas_it, "ip");
      JSON_ASSERT_STRING((*sas_it)["ip"]);

      _single_sas_server = (*sas_it)["ip"].GetString();
    }

    // We have a valid rapidjson object.  Write this to the _sas_servers member
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    sas_servers.Accept(writer);

    TRC_DEBUG("New _sas_servers config:  %s", buffer.GetString());
    _sas_servers = buffer.GetString();
  }
  catch (JsonFormatError err)
  {
    TRC_ERROR("Badly formed SAS configuration file");
  }
}

SasService::~SasService()
{
  // Destroy the updater (if it was created).
  delete _updater;
  _updater = NULL;
}
