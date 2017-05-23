/**
 * @file xml_utils.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef XML_UTILS_H__
#define XML_UTILS_H__

#include "rapidxml/rapidxml.hpp"

#include <string>
#include <vector>
#include <memory>

using namespace rapidxml;

namespace XMLUtils
{
  // Utility functions to parse out XML structures
  long parse_integer(xml_node<>* node,
                     std::string description,
                     long min_value,
                     long max_value);
  bool parse_bool(xml_node<>* node,
                  std::string description);
  std::string get_first_node_value(xml_node<>* node,
                                   std::string name);
  std::string get_text_or_cdata(xml_node<>* node);
  bool does_child_node_exist(xml_node<>* parent_node,
                             std::string child_node_name);
}

/// Exception thrown internally during XML parsing.
class xml_error : public std::exception
{
public:
  xml_error(std::string what)
    : _what(what)
  {
  }

  virtual ~xml_error() throw ()
  {
  }

  virtual const char* what() const throw()
  {
    return _what.c_str();
  }

private:
  std::string _what;
};

// Functions and constants specific to parsing the RegData XML (used by
// Sprout and Homestead).
namespace RegDataXMLUtils
{
  const char* const PRIORITY = "Priority";
  const char* const CLEARWATER_REG_DATA = "ClearwaterRegData";
  const char* const REGISTRATION_STATE = "RegistrationState";
  const char* const CHARGING_ADDRESSES = "ChargingAddresses";
  const char* const CCF = "CCF";
  const char* const ECF = "ECF";
  const char* const IMS_SUBSCRIPTION = "IMSSubscription";
  const char* const SERVICE_PROFILE = "ServiceProfile";
  const char* const PUBLIC_IDENTITY = "PublicIdentity";
  const char* const IDENTITY = "Identity";
  const char* const BARRING_INDICATION = "BarringIndication";
  const char* const PRIVATE_ID = "PrivateID";
  const char* const EXTENSION = "Extension";
  const char* const IDENTITY_TYPE = "IdentityType";
  const char* const WILDCARDED_PSI = "WildcardedPSI";
  const char* const WILDCARDED_IMPU = "WildcardedIMPU";
  const char* const CONDITION_NEGATED = "ConditionNegated";
  const char* const GROUP = "Group";
  const char* const METHOD = "Method";
  const char* const SPT = "SPT";
  const char* const REGISTRATION_TYPE = "RegistrationType";
  const char* const SIP_HEADER = "SIPHeader";
  const char* const HEADER = "Header";
  const char* const CONTENT = "Content";
  const char* const SESSION_CASE = "SessionCase";
  const char* const REQUEST_URI = "RequestURI";
  const char* const SESSION_DESCRIPTION = "SessionDescription";
  const char* const LINE = "Line";
  const char* const APPLICATION_SERVER = "ApplicationServer";
  const char* const SERVER_NAME = "ServerName";
  const char* const PROFILE_PART_INDICATOR = "ProfilePartIndicator";
  const char* const TRIGGER_POINT = "TriggerPoint";
  const char* const CONDITION_TYPE_CNF = "ConditionTypeCNF";
  const char* const DEFAULT_HANDLING = "DefaultHandling";
  const char* const INC_REG_REQ = "IncludeRegisterRequest";
  const char* const INC_REG_RSP = "IncludeRegisterResponse";
  const char* const SERVICE_INFO = "ServiceInfo";
  const char* const IFC = "InitialFilterCriteria";
  const char* const SIFC = "SharedIFCSetID";
  const char* const IDENTITY_TYPE_IMPU = "0";
  const char* const IDENTITY_TYPE_PSI = "1";
  const char* const IDENTITY_TYPE_WILDCARD_PSI = "2";
  const char* const IDENTITY_TYPE_NON_DISTINCT_IMPU = "3";
  const char* const IDENTITY_TYPE_WILDCARD_IMPU = "4";
  const char* const STATE_NOT_REGISTERED = "NOT_REGISTERED";
  const char* const STATE_REGISTERED = "REGISTERED";
  const char* const STATE_UNREGISTERED = "UNREGISTERED";
  const char* const STATE_UNBARRED = "0";
  const char* const STATE_BARRED = "1";
  const char* const CCF_ECF_PRIORITY = "priority";
  const char* const CCF_PRIORITY_1 = "1";
  const char* const CCF_PRIORITY_2 = "2";
  const char* const ECF_PRIORITY_1 = "1";
  const char* const ECF_PRIORITY_2 = "2";

  void parse_extension_identity(std::string& uri,
                                rapidxml::xml_node<>* extension);
}

#endif
