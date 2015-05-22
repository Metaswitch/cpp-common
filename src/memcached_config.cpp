/**
 * @file memcached_config.cpp Classes to handle reading and parsing memcached
 * config.
 *
 * Project Clearwater - IMS in the cloud.
 * Copyright (C) 2015  Metaswitch Networks Ltd
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

#include <fstream>

#include "log.h"
#include "utils.h"
#include "memcached_config.h"

/// The default lifetime (in seconds) of tombstones written to memcached.
static const int DEFAULT_TOMBSTONE_LIFETIME = 0;

MemcachedConfig::MemcachedConfig() :
  servers(), new_servers(), tombstone_lifetime(DEFAULT_TOMBSTONE_LIFETIME)
{}


MemcachedConfigFileReader::MemcachedConfigFileReader(const std::string& filename) :
  _filename(filename)
{}

MemcachedConfigFileReader::~MemcachedConfigFileReader() {}

bool MemcachedConfigFileReader::read_config(MemcachedConfig& config)
{
  bool seen_servers = false;
  config.servers.clear();
  config.new_servers.clear();
  config.tombstone_lifetime = DEFAULT_TOMBSTONE_LIFETIME;

  std::ifstream f(_filename);

  if (f.is_open() && f.good())
  {
    LOG_STATUS("Reloading memcached configuration from '%s'", _filename.c_str());

    while (f.good())
    {
      std::string line;
      getline(f, line);

      LOG_DEBUG("Got line: %s", line.c_str());

      if (line.length() > 0)
      {
        // Read a non-blank line.
        std::vector<std::string> tokens;
        Utils::split_string(Utils::strip_whitespace(line), 
                            '=', 
                            tokens, 
                            0, 
                            true);

        std::string key;
        std::string value;
        if (tokens.size() == 1)
        {
          key = tokens[0];
          value = "";
        }
        else if (tokens.size() == 2)
        {
          key = tokens[0];
          value = tokens[1];
        }
        else
        {
          LOG_ERROR("Malformed config file (got bad line: '%s')",
                    line.c_str());
          return false;
        }

        LOG_STATUS(" %s=%s", key.c_str(), value.c_str());

        if (key == "servers")
        {
          // Found line defining servers.
          Utils::split_string(value, ',', config.servers, 0, true);
          seen_servers = true;
        }
        else if (key == "new_servers")
        {
          // Found line defining new servers.
          Utils::split_string(value, ',', config.new_servers, 0, true);
        }
        else if (key == "tombstone_lifetime")
        {
          // Read the tombstone lifetime from the config file. Check it is
          // actually a valid integer before committing to the member variable
          // (atoi stops when it reaches non-numeric characters).
          config.tombstone_lifetime = atoi(value.c_str());

          if (std::to_string(config.tombstone_lifetime) != value)
          {
            LOG_ERROR("Config contained an invalid tombstone_lifetime line:\n%s",
                      line.c_str());
            return false;
          }
        }
         else
        {
          LOG_ERROR("Malformed config file (got bad line: '%s')",
                    line.c_str());
          return false;
        }
      }
    }
  }
  else
  {
    LOG_ERROR("Failed to open '%s'", _filename.c_str());
    return false;
  }

  return seen_servers;
}
