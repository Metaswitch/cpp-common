/**
 * @file memcached_config.h Classes to handle reading and parsing memcached
 * configuration.
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

#ifndef MEMCACHED_CONFIG_H_
#define MEMCACHED_CONFIG_H_

#include <vector>
#include <string>


/// Structure holding memcached configuration parameters.
struct MemcachedConfig
{
  /// The servers in the cluster. Each entry is an IP address and port
  /// combination (e.g.  "192.168.0.1:11211" or "[1111:2222::]:55555").
  std::vector<std::string> servers;

  /// During a scale-up or scale-down operation, the servers that will be in
  /// the cluster once the operation has completed. Empty if no scale-up/down
  /// is in progress.
  ///
  /// Each entry is an IP address and port combination.
  std::vector<std::string> new_servers;

  /// The expiry time (in seconds) of tombstone records that are written when
  /// data is deleted from memcached.
  ///
  /// 0 means that tombstones are not used. Deleted data is simply removed.
  int tombstone_lifetime;

  /// Default constructor.
  MemcachedConfig();
};


/// Interface that all memcached config readers must implement.
class MemcachedConfigReader
{
public:
  /// Read the current memcached config.
  ///
  /// @return       - True if the config was read successfully. False otherwise.
  ///
  /// @param config - (out) The config that was read.  This is only valid if
  ///                 the function returns true.
  virtual bool read_config(MemcachedConfig& config) = 0;

  /// Virtual destructor.
  virtual ~MemcachedConfigReader() {};
};


/// Class that reads the memcached config from a file on disk.
class MemcachedConfigFileReader : public MemcachedConfigReader
{
public:
  /// Constructor
  ///
  /// @param filename - The full filename (path and name) to read the config
  /// from.
  MemcachedConfigFileReader(const std::string& filename);

  ~MemcachedConfigFileReader();

  bool read_config(MemcachedConfig& config);

private:
  std::string _filename;
};

#endif

