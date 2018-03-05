/**
 * @file alarmdefinition.cpp
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "alarmdefinition.h"

namespace AlarmDef {

  Cause cause_to_enum(std::string cause)
  {
    std::string cause_to_lower = cause;
    boost::algorithm::to_lower(cause_to_lower);
    if (cause_to_lower == "database_inconsistency")
    {
      return DATABASE_INCONSISTENCY;
    }
    else if (cause_to_lower == "software_error")
    {
      return SOFTWARE_ERROR;
    }
    else if (cause_to_lower == "underlying_resource_unavailable")
    {
      return UNDERLYING_RESOURCE_UNAVAILABLE;
    }
    else
    {
      return UNDEFINED_CAUSE;
    }
  }

  Severity severity_to_enum(std::string severity)
  {
    std::string severity_to_lower = severity;
    boost::algorithm::to_lower(severity_to_lower);
    if (severity_to_lower == "cleared")
    {
      return CLEARED;
    }
    else if (severity_to_lower == "critical")
    {
      return CRITICAL;
    }
    else if (severity_to_lower == "indeterminate")
    {
      return INDETERMINATE;
    }
    else if (severity_to_lower == "major")
    {
      return MAJOR;
    }
    else if (severity_to_lower == "minor")
    {
      return MINOR;
    }
    else if (severity_to_lower == "warning")
    {
      return WARNING;
    }
    else
    {
      return UNDEFINED_SEVERITY;
    }
  }
}
