/**
 * @file store.h  Abstract base class defining interface to Sprout data store.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef STORE_H_
#define STORE_H_
#include "sas.h"

/// @class Store
///
/// Abstract base class for the Sprout data store.  This can be used to store
/// data that must be shared across the Sprout cluster.
///
class Store
{
public:
  /// Must define a destructor, even though it does nothing, to ensure there
  /// is an entry for it in the vtable.
  virtual ~Store()
  {
  }

  /// Status used to indicate success or failure of store operations.
  typedef enum {OK=1, NOT_FOUND=2, DATA_CONTENTION=3, ERROR=4} Status;

  /// Gets the data for the specified key in the specified namespace.
  ///
  /// @return         Status value indicating the result of the read.
  /// @param table    Name of the table to retrive the data.
  /// @param key      Key of the data record to retrieve.
  /// @param data     String to return the data.
  /// @param cas      Variable to return the CAS value of the data.
  virtual Status get_data(const std::string& table,
                          const std::string& key,
                          std::string& data,
                          uint64_t& cas,
                          SAS::TrailId trail = 0) = 0;

  /// Sets the data for the specified key in the specified namespace.
  ///
  /// @return         Status value indicating the result of the write.
  /// @param table    Name of the table to store the data.
  /// @param key      Key used to index the data within the table.
  /// @param data     Data to store.
  /// @param cas      CAS (Check-and-Set) value for the data.  Should be set
  ///                 to the CAS value returned when the data was read, or
  ///                 zero if writing a record for the first time.
  /// @param expiry   Expiry period of the data (in seconds).  If zero the
  ///                 data will expire immediately.
  virtual Status set_data(const std::string& table,
                          const std::string& key,
                          const std::string& data,
                          uint64_t cas,
                          int expiry,
                          SAS::TrailId trail = 0) = 0;

  /// Delete the data for the specified key in the specified namespace.
  ///
  /// @return         Status value indicating the result of the delete.
  /// @param table    Name of the table to store the data.
  /// @param key      Key used to index the data within the table.
  virtual Status delete_data(const std::string& table,
                             const std::string& key,
                             SAS::TrailId trail = 0) = 0;

  virtual bool has_servers() { return true; }
};

#endif
