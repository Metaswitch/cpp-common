/**
 * @file localstore.h Definitions for the LocalStore class
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef LOCALSTORE_H__
#define LOCALSTORE_H__

#include <map>
#include <pthread.h>

#include "store.h"

class LocalStore : public Store
{
public:
  LocalStore();
  virtual ~LocalStore();

  void flush_all();
  void force_contention();
  void force_error();

  Store::Status get_data(const std::string& table,
                         const std::string& key,
                         std::string& data,
                         uint64_t& cas,
                         SAS::TrailId trail = 0);
  Store::Status set_data(const std::string& table,
                         const std::string& key,
                         const std::string& data,
                         uint64_t cas,
                         int expiry,
                         SAS::TrailId trail = 0);
  Store::Status delete_data(const std::string& table,
                            const std::string& key,
                            SAS::TrailId trail = 0);
private:
  typedef struct record
  {
    std::string data;
    uint32_t expiry;
    uint64_t cas;
  } Record;
  bool _data_contention_flag;
  pthread_mutex_t _db_lock;
  std::map<std::string, Record> _db;
  bool _force_error_flag;
  std::map<std::string, Record> _old_db;
};


#endif
