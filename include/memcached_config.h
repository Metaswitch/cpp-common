/**
 * @file memcached_config.h Classes to handle reading and parsing memcached
 * configuration.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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

  /// The configuration file used for this memcached cluster. Used for logging.
  std::string filename;

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

