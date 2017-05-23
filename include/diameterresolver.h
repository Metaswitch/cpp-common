/**
 * @file diameterresolver.h  Declaration of Diameter DNS resolver class.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef DIAMETERRESOLVER_H__
#define DIAMETERRESOLVER_H__

#include "baseresolver.h"
#include "sas.h"

class DiameterResolver : public BaseResolver
{
public:
  DiameterResolver(DnsCachedResolver* dns_client,
                   int af,
                   int blacklist_duration = DEFAULT_BLACKLIST_DURATION);
  ~DiameterResolver();

  virtual void resolve(const std::string& realm,
                       const std::string& host,
                       int max_targets,
                       std::vector<AddrInfo>& targets,
                       int& ttl);

  /// Default duration to blacklist hosts after we fail to connect to them.
  static const int DEFAULT_BLACKLIST_DURATION = 30;

  static const int DEFAULT_PORT = 3868;
  static const int DEFAULT_TRANSPORT = IPPROTO_SCTP;

private:
  int _address_family;
};

#endif
