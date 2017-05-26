/**
 * @file localstore.cpp Local memory implementation of the Sprout data store.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

// Common STL includes.
#include <cassert>
#include <map>
#include <list>
#include <string>

#include <time.h>
#include <stdint.h>

#include "log.h"
#include "localstore.h"


LocalStore::LocalStore() :
  _data_contention_flag(false),
  _db_lock(PTHREAD_MUTEX_INITIALIZER),
  _db(),
  _force_error_on_set_flag(false),
  _force_error_on_get_flag(false),
  _old_db()
{
  TRC_DEBUG("Created local store");
}


LocalStore::~LocalStore()
{
  flush_all();
  pthread_mutex_destroy(&_db_lock);
}


void LocalStore::flush_all()
{
  pthread_mutex_lock(&_db_lock);
  TRC_DEBUG("Flushing local store");
  _db.clear();
  pthread_mutex_unlock(&_db_lock);
}

//This function sets a flag to true that tells the program to simulate data
//contention for testing. We achieve this by creating an out-of-date database
//(_old_db) in //set_data() and reading from this old database in get_data()
// if the flag is true.
void LocalStore::force_contention()
{
  _data_contention_flag = true;
}

// This function sets a flag to true that tells the program to simulate an
// error on the SET.  We achieve this by just returning an error on the SET.
void LocalStore::force_error()
{
  _force_error_on_set_flag = true;
}

// This function sets a flag to true that tells the program to simulate an
// error on the GET.  We achieve this by just returning an error on the GET.
void LocalStore::force_get_error()
{
  _force_error_on_get_flag = true;
}

Store::Status LocalStore::get_data(const std::string& table,
                                   const std::string& key,
                                   std::string& data,
                                   uint64_t& cas,
                                   SAS::TrailId trail)
{
  TRC_DEBUG("get_data table=%s key=%s", table.c_str(), key.c_str());
  Store::Status status = Store::Status::NOT_FOUND;

  // This is for the purpose of testing data GETs failing.  If the flag is set
  // to true, then we'll just return an error.
  if (_force_error_on_get_flag)
  {
    TRC_DEBUG("Force an error on the GET");
    _force_error_on_get_flag = false;
    return Store::Status::ERROR;
  }

  // Calculate the fully qualified key.
  std::string fqkey = table + "\\\\" + key;

  pthread_mutex_lock(&_db_lock);

  // This is for the purposes of testing data contention. If the flag is set to
  // true _db_in_use will become a reference to _old_db the out-of-date
  // database we constructed in set_data().
  std::map<std::string, Record>& _db_in_use = _data_contention_flag ? _old_db : _db;
  if (_data_contention_flag)
  {
    _data_contention_flag = false;
  }

  uint32_t now = time(NULL);

  TRC_DEBUG("Search store for key %s", fqkey.c_str());

  std::map<std::string, Record>::iterator i = _db_in_use.find(fqkey);
  if (i != _db_in_use.end())
  {
    // Found an existing record, so check the expiry.
    Record& r = i->second;
    TRC_DEBUG("Found record, expiry = %ld (now = %ld)", r.expiry, now);
    if (r.expiry < now)
    {
      // Record has expired, so remove it from the map and return not found.
      TRC_DEBUG("Record has expired, remove it from store");
      _db_in_use.erase(i);
    }
    else
    {
      // Record has not expired, so return the data and the cas value.
      TRC_DEBUG("Record has not expired, return %d bytes of data with CAS = %ld",
                r.data.length(), r.cas);
      data = r.data;
      cas = r.cas;
      status = Store::Status::OK;
    }
  }

  pthread_mutex_unlock(&_db_lock);

  TRC_DEBUG("get_data status = %d", status);

  return status;
}


Store::Status LocalStore::set_data(const std::string& table,
                                   const std::string& key,
                                   const std::string& data,
                                   uint64_t cas,
                                   int expiry,
                                   SAS::TrailId trail)
{
  TRC_DEBUG("set_data table=%s key=%s CAS=%ld expiry=%d",
            table.c_str(), key.c_str(), cas, expiry);

  Store::Status status = Store::Status::DATA_CONTENTION;

  // This is for the purpose of testing data SETs failing.  If the flag is set
  // to true, then we'll just return an error.
  if (_force_error_on_set_flag)
  {
    TRC_DEBUG("Force an error on the SET");
    _force_error_on_set_flag = false;

    return Store::Status::ERROR;
  }

  // Calculate the fully qualified key.
  std::string fqkey = table + "\\\\" + key;

  pthread_mutex_lock(&_db_lock);

  uint32_t now = time(NULL);

  TRC_DEBUG("Search store for key %s", fqkey.c_str());

  std::map<std::string, Record>::iterator i = _db.find(fqkey);

  if (i != _db.end())
  {
    // Found an existing record, so check the expiry and CAS value.
    Record& r = i->second;
    TRC_DEBUG("Found existing record, CAS = %ld, expiry = %ld (now = %ld)",
              r.cas, r.expiry, now);

    if (((r.expiry >= now) && (cas == r.cas)) ||
        ((r.expiry < now) && (cas == 0)))
    {
      // Supplied CAS is consistent (either because record hasn't expired and
      // CAS matches, or record has expired and CAS is zero) so update the
      // record.

      // This writes data this is one update out-of-date to _old_db. This is for
      // the purposes of simulating data contention in Unit Testing.
      _old_db[fqkey] = r;

      r.data = data;
      r.cas = ++cas;
      r.expiry = (expiry == 0) ? 0 : (uint32_t)expiry + now;
      status = Store::Status::OK;
      TRC_DEBUG("CAS is consistent, updated record, CAS = %ld, expiry = %ld (now = %ld)",
                r.cas, r.expiry, now);
    }
  }
  else if (cas == 0)
  {
    // No existing record and supplied CAS is zero, so add a new record.
    Record& r = _db[fqkey];
    r.data = data;
    r.cas = 1;
    r.expiry = (expiry == 0) ? 0 : (uint32_t)expiry + now;
    status = Store::Status::OK;
    TRC_DEBUG("No existing record so inserted new record, CAS = %ld, expiry = %ld (now = %ld)",
              r.cas, r.expiry, now);
  }

  pthread_mutex_unlock(&_db_lock);
  return status;
}

Store::Status LocalStore::delete_data(const std::string& table,
                                      const std::string& key,
                                      SAS::TrailId trail)
{
  TRC_DEBUG("delete_data table=%s key=%s",
            table.c_str(), key.c_str());

  Store::Status status = Store::Status::OK;

  // Calculate the fully qualified key.
  std::string fqkey = table + "\\\\" + key;

  pthread_mutex_lock(&_db_lock);

  _db.erase(fqkey);

  pthread_mutex_unlock(&_db_lock);

  return status;
}

void LocalStore::swap_dbs(LocalStore* rhs)
{
  // Grab both DB locks. Technically this could cause a deadlock (if another
  // thread calls swap_dbs on the rhs) but we only use this in test code
  // anyway.
  pthread_mutex_lock(&_db_lock);
  pthread_mutex_lock(&rhs->_db_lock);

  std::swap(_db, rhs->_db);
  std::swap(_old_db, rhs->_old_db);

  pthread_mutex_unlock(&rhs->_db_lock);
  pthread_mutex_unlock(&_db_lock);
}

