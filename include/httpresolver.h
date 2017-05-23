/**
 * @file httpresolver.h  Declaration of HTTP DNS resolver class.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef HTTPRESOLVER_H_
#define HTTPRESOLVER_H_

#include "a_record_resolver.h"

class HttpResolver : public ARecordResolver
{
public:
  HttpResolver(DnsCachedResolver* dns_client,
               int address_family,
               int blacklist_duration = DEFAULT_BLACKLIST_DURATION,
               int graylist_duration = DEFAULT_GRAYLIST_DURATION)
    : ARecordResolver(dns_client,
                      address_family,
                      blacklist_duration,
                      graylist_duration,
                      DEFAULT_HTTP_PORT)
  {
  }

private:
  static const int DEFAULT_HTTP_PORT = 80;
};

#endif
