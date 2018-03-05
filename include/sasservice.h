/**
 * @file sasservice.h class definition
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef SASSERVICE_H
#define SASSERVICE_H

#include <map>
#include <string>
#include <boost/regex.hpp>
#include <boost/thread.hpp>

#include <functional>
#include "updater.h"
#include "sas.h"

class SasService
{
public:
  SasService(std::string configuration = "/etc/clearwater/sas.json");
  ~SasService();

  void reload_config();
  void extract_config();

  std::string get_single_sas_server() {return _last_sas_server;};
  std::string get_sas_servers() {return _sas_servers;};
private:
  std::string _configuration;
  std::string _last_sas_server = "0.0.0.0";
  std::string _sas_servers = "[]";
  Updater<void, SasService>* _updater;
};

#endif
