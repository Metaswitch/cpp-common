/**
 * @file snmp_sip_request_types.h
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef SNMP_SIP_REQUEST_TYPES_H
#define SNMP_SIP_REQUEST_TYPES_H

namespace SNMP
{
enum SIPRequestTypes
{
  INVITE = 0,
  ACK = 1,
  BYE = 2,
  CANCEL = 3, 
  OPTIONS = 4, 
  REGISTER = 5,
  PRACK = 6,
  SUBSCRIBE = 7,
  NOTIFY = 8,
  PUBLISH = 9,
  INFO = 10,
  REFER = 11, 
  MESSAGE = 12, 
  UPDATE = 13, 
  OTHER = 14,
};

SIPRequestTypes string_to_request_type(char* req_type, int slen);

}

#endif
