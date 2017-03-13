/**
 * @file wildcard_utils.cpp
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

#include <vector>
#include <boost/regex.hpp>
#include "wildcard_utils.h"
#include "utils.h"
#include "log.h"

bool WildcardUtils::is_wildcard_uri(const std::string& possible_wildcard)
{
  // This function counts how many !s there are in the URI string
  // before the @ - note, this only checks whether the URI string
  // must represent a wildcarded URI if it's a valid URI - it
  // doesn't say whether the returned URI is actually valid.
  //
  // We use string manipulation for this rather than parsing it as a URI
  // using PJSIP, as we don't know if this actually corresponds to a
  // valid URI, and adding more validation to the URI would be too
  // heavyweight.
  std::vector<std::string> wildcard_parts;
  Utils::split_string(possible_wildcard, '@', wildcard_parts, 0, false);
  return (std::count(wildcard_parts[0].begin(),
                     wildcard_parts[0].end(),
                     '!') >= 2);
}

bool WildcardUtils::check_users_equivalent(const std::string& wildcard_user,
                                           const std::string& specific_user)
{
  if (wildcard_user == specific_user)
  {
    // Check if the identities are already the same without looking at the
    // wildcards.
    return true;
  }
  else if ((wildcard_user == "") || (specific_user == ""))
  {
    // Check if either string is empty (where we've caught the case where
    // they're both empty at the start) as this definitely won't match, and
    // checking now simplifies the error checking logic later.
    return false;
  }

  // We don't match on any parameters in the URI, so strip them out before
  // doing anymore processing. Then check again if the identities are the same
  // now that we don't have any parameters.
  std::vector<std::string> wildcard_user_parts;
  std::vector<std::string> specific_user_parts;
  Utils::split_string(wildcard_user, ';', wildcard_user_parts, 0, false);
  Utils::split_string(specific_user, ';', specific_user_parts, 0, false);

  if (wildcard_user_parts[0] == specific_user_parts[0])
  {
    return true;
  }

  // Only chance to match now is if the wildcard_user is a wildcard. The wildcard
  // has the format:
  //
  //    <non wildcard part>!<regex>!<non wildcard part>
  //
  // Either of the wildcard parts or the regex can be empty.
  // We have to split out the three parts of the wildcard, then check the first
  // part directly matches the start of the specific_user string, the end part
  // directly matches the end of the specific_user string, and the regex matches
  // against whatever's left of the specific_user string.
  std::size_t wildcard_start = wildcard_user_parts[0].find_first_of("!");
  std::size_t wildcard_end = wildcard_user_parts[0].find_last_of("!");

  if ((wildcard_start == std::string::npos) ||
      (wildcard_end == std::string::npos) ||
      (wildcard_start == wildcard_end))
  {
    // The wildcard_user isn't a wildcard
    return false;
  }

  // Check the start and end parts
  std::string wildcard_start_str = wildcard_user_parts[0].substr(0, wildcard_start);
  std::string wildcard_end_str = wildcard_user_parts[0].substr(wildcard_end + 1);
  std::string specific_start_str = specific_user_parts[0].substr(0, wildcard_start);
  std::string specific_end_str = specific_user_parts[0].substr(specific_start_str.size());
  specific_end_str = (specific_end_str.size() >= wildcard_end_str.size()) ?
   specific_end_str.substr(specific_end_str.size() - wildcard_end_str.size()) :
   specific_end_str;

  if ((specific_start_str != wildcard_start_str) ||
      (specific_end_str != wildcard_end_str))
  {
    // Either the start or end of the wildcard didn't directly match the
    // specific_user string
    return false;
  }

  // Finally, check against the regex.
  std::string wildcard_part = wildcard_user_parts[0].substr(wildcard_start + 1,
                                                            (wildcard_end -
                                                             wildcard_start -
                                                             1));
  std::string specific_part = specific_user_parts[0].substr(
                                                specific_start_str.size(),
                                                (specific_user_parts[0].size() -
                                                 specific_end_str.size() -
                                                 specific_start_str.size()));
  boost::regex wildcard_regex = boost::regex(wildcard_part,
                                             boost::regex_constants::no_except);

  if ((!wildcard_regex.status()) &&
      (boost::regex_match(specific_part, wildcard_regex)))
  {
    return true;
  }
  else
  {
    return false;
  }
}
