/**
 * @file xml_utils.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <cstring>

#include "xml_utils.h"
#include "log.h"

namespace XMLUtils
{

// Gets the first child node of "node" with name "name". Returns an empty string
// if there is no such node, otherwise returns its value (which is the empty
// string if it has no value).
std::string get_first_node_value(xml_node<>* node, std::string name)
{
  xml_node<>* first_node = node->first_node(name.c_str());
  if (!first_node)
  {
    return "";
  }
  else
  {
    return get_text_or_cdata(first_node);
  }
}

// Takes an XML node containing ONLY text or CDATA (not both) and returns the
// value of that text or CDATA.
//
// This is necessary because RapidXML's value() function only returns the text
// of the first data node, not the first CDATA node.
std::string get_text_or_cdata(xml_node<>* node)
{
  xml_node<>* first_data_node = node->first_node();
  if (first_data_node && ((first_data_node->type() != node_cdata) ||
                          (first_data_node->type() != node_data)))
  {
    return first_data_node->value();
  }
  else
  {
    return "";
  }
}

bool does_child_node_exist(xml_node<>* parent_node, std::string child_node_name)
{
  xml_node<>* child_node = parent_node->first_node(child_node_name.c_str());
  return (child_node != NULL);
}

// Attempt to parse the content of the node as a bounded integer
// returning the result or throwing.
long parse_integer(xml_node<>* node,
                   std::string description,
                   long min_value,
                   long max_value)
{
  // Node must be non-NULL - caller should check for this prior to calling
  // this method.
  assert(node != NULL);

  const char* nptr = node->value();
  char* endptr = NULL;
  long int n = strtol(nptr, &endptr, 10);

  if ((*nptr == '\0') || (*endptr != '\0'))
  {
    throw xml_error("Can't parse " + description + " as integer");
  }

  if ((n < min_value) || (n > max_value))
  {
    throw xml_error(description + " out of allowable range " +
                 std::to_string(min_value) + ".." + std::to_string(max_value));
  }

  return n;
}

/// Parse an xs:boolean value.
bool parse_bool(xml_node<>* node, std::string description)
{
  if (!node)
  {
    throw xml_error("Missing mandatory value for " + description);
  }

  const char* nptr = node->value();

  return ((strcmp("true", nptr) == 0) || (strcmp("1", nptr) == 0));
}
};

namespace RegDataXMLUtils
{

// The PublicIdentity element in a ServiceProfile typically has the following
// syntax
//
//  <PublicIdentity>
//    <Identity>SIP or Tel URI</Identity>
//  </PublicIdentity>
//
// If the identity is a distinct IMPU, wildcard IMPU, distinct PSI or wildcard
// PSI then we always want to take the identity from the top level Identity
// element. If the identity is a non-distinct IMPU then we need to take the
// identity from the WildcardedIMPU element, where the syntax is:
//
//  <PublicIdentity>
//    <Identity>SIP or Tel URI</Identity>
//    <Extension>
//      <IdentityType>3</IdentityType>
//      <Extension>
//        <Extension>
//          <WildcardedIMPU>SIP or Tel URI</WildcardedIMPU>
//        </Extension>
//      </Extension>
//    </Extension>
//  </PublicIdentity>
//
void parse_extension_identity(std::string& uri, rapidxml::xml_node<>* extension)
{
  rapidxml::xml_node<>* id_type =
                          extension->first_node(RegDataXMLUtils::IDENTITY_TYPE);

  if ((id_type) && (std::string(id_type->value()) ==
                    RegDataXMLUtils::IDENTITY_TYPE_NON_DISTINCT_IMPU))
  {
    rapidxml::xml_node<>* extension_1 =
                              extension->first_node(RegDataXMLUtils::EXTENSION);

    if (extension_1)
    {
      rapidxml::xml_node<>* extension_2 =
                            extension_1->first_node(RegDataXMLUtils::EXTENSION);

      if (extension_2)
      {
        rapidxml::xml_node<>* new_identity =
                      extension_2->first_node(RegDataXMLUtils::WILDCARDED_IMPU);

        if (new_identity)
        {
          uri = std::string(new_identity->value());
        }
      }
    }
  }
}
};
