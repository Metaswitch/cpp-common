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
#include <functional>

#include <boost/thread.hpp>


class SasService
{
public:
  SasService(std::string system_name, std::string system_type, bool sas_signaling_if, std::string configuration = "/etc/clearwater/sas.json");
  ~SasService();

  std::string get_single_sas_server();
  std::string get_sas_servers();
private:
  void extract_config();

  std::string _configuration;
  std::string _sas_servers;
  std::string _single_sas_server;

  boost::shared_mutex _sas_servers_lock;
  boost::shared_mutex _single_sas_server_lock;
};

#endif
