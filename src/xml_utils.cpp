/**
 * @file xml_utils.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2017  Metaswitch Networks Ltd
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

  TRC_DEBUG("%lu", n);
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
