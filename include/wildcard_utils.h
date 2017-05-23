/**
 * @file wildcard_utils.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef WILDCARD_UTILS_H_
#define WILDCARD_UTILS_H_

#include <string>

namespace WildcardUtils
{
  // Checks if a string represents a wildcard URI.
  bool is_wildcard_uri(const std::string& possible_wildcard);

  // Checks if two URIs match, including wildcard checking.
  bool check_users_equivalent(const std::string& wildcard_user,
                              const std::string& specific_user);

} // namespace WildcardUtils

#endif /* WILDCARD_UTILS_H_ */
