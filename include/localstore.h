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
  void force_get_error();
  void force_delete_error();
  void swap_dbs(LocalStore* rhs);

  using Store::get_data;
  using Store::set_data;

  Store::Status get_data(const std::string& table,
                         const std::string& key,
                         std::string& data,
                         uint64_t& cas,
                         SAS::TrailId trail,
                         bool log_body,
                         Store::Format data_format=Store::Format::HEX) override;

  Store::Status set_data(const std::string& table,
                         const std::string& key,
                         const std::string& data,
                         uint64_t cas,
                         int expiry,
                         SAS::TrailId trail,
                         bool log_body,
                         Store::Format data_format=Store::Format::HEX) override;

  Store::Status set_data_without_cas(const std::string& table,
                                     const std::string& key,
                                     const std::string& data,
                                     int expiry,
                                     SAS::TrailId trail,
                                     bool log_body,
                                     Store::Format data_format=Store::Format::HEX) override;

  Store::Status delete_data(const std::string& table,
                            const std::string& key,
                            SAS::TrailId trail = 0) override;
private:
  Store::Status set_data_inner(const std::string& table,
                               const std::string& key,
                               const std::string& data,
                               uint64_t cas,
                               bool check_cas,
                               int expiry,
                               SAS::TrailId trail = 0);
  typedef struct record
  {
    std::string data;
    uint32_t expiry;
    uint64_t cas;
  } Record;
  bool _data_contention_flag;
  pthread_mutex_t _db_lock;
  std::map<std::string, Record> _db;
  bool _force_error_on_set_flag;
  bool _force_error_on_get_flag;
  bool _force_error_on_delete_flag;
  std::map<std::string, Record> _old_db;
};


#endif
