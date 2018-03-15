/**
 * @file ttlcache.h  Templated implementation of a TTL cache.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef TTLCACHE_H__
#define TTLCACHE_H__

#include <pthread.h>

#include <map>
#include <memory>

#include "log.h"

/// Factory base class for cache.
template <class K, class V>
class CacheFactory
{
public:
  virtual std::shared_ptr<V> get(K key, int& ttl, SAS::TrailId trail) = 0;
};

/// Templated class implementing a cache of items with a specified TTL.  Users
/// registed a factory when the cache is created which is used to greate entries
/// on cache misses.  The cache is thread-safe and guarantees that multiple
/// concurrent calls to get for the same key will only result in a single call
/// to the factory.
///
/// When the destructor for the class used for Value is called, it must free all
/// resources associated with that Value object, since that object is only
/// accessed by shared_ptr's.
template <class K, class V>
class TTLCache
{
  /// The expiry list is a multimap indexed on expiry time (in seconds since
  /// epoch).
  typedef std::multimap<time_t, K> ExpiryList;
  typedef typename ExpiryList::iterator ExpiryIterator;

  /// The cache itself is a map indexed on the key, where each entry contains
  /// a shared pointer to the value plus various housekeeping fields ...
  /// -   state and lock fields used to ensure that each cache entry is only
  ///     populated once even if multiple threads try to get it at the same
  ///     time
  /// -   the iterator of the item in the expiry list (or expiry_list.end() if
  ///     it is not yet in the list.
  struct Entry
  {
    enum {PENDING, COMPLETE} state;
    ExpiryIterator expiry_i;
    std::shared_ptr<V> data_ptr;
  };

  typedef std::map<K, Entry> KeyMap;
  typedef typename KeyMap::iterator KeyMapIterator;

public:
  /// factory is assumed to not be null.
  TTLCache(CacheFactory<K, V>* factory) :
    _factory(factory),
    _lock(PTHREAD_MUTEX_INITIALIZER),
    _expiry_list(),
    _cache()
  {
    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
    pthread_cond_init(&_cond, &cond_attr);
    pthread_condattr_destroy(&cond_attr);
  }

  ~TTLCache()
  {
    pthread_mutex_destroy(&_lock);
    pthread_cond_destroy(&_cond);
  }

  /// Get or create an entry in the cache.
  std::shared_ptr<V> get(K key, int& ttl, SAS::TrailId trail)
  {
    bool retry_cache_lookup = true;
    std::shared_ptr<V> data_ptr = NULL;

    // Take the lock. We need to lock anytime we do anything with cache
    // entries
    pthread_mutex_lock(&_lock);

    // Evict any old entries.
    evict();

    KeyMapIterator kmi = _cache.find(key);

    // We need to hold the lock if we're looking at cache entries, as otherwise
    // the evict method could be called which potentially destroys the entry.
    // However, we can't hold the lock while we're actually getting the DNS
    // record as this blocks for too long. Instead the logic is:
    // - Look in the cache. The entry is either:
    //   - Not present:
    //     - Create an entry in the cache with the state set to PENDING.
    //     - Store a shared_ptr to the DNS result. We have to release the lock
    //       at this stage.
    //     - We've now got the DNS result. As we'd released the lock we can
    //       no longer trust the entry we had at the start. Instead, re-get
    //       the entry from the cache, creating it again if necessary. Set
    //       the expiry time and the state to COMPLETE.
    //     - Release the lock
    //     - Return the shared_ptr
    //   - Present in state pending:
    //     - Wait on the lock. This releases the lock, then reclaims it when
    //       it's next free
    //     - As we've released the lock we can't look at the entry we had at
    //       the start. Instead we just start again.
    //     - There's an issue here where there's only one lock for every cache
    //       entry. This means that this logic can be repeated many times before
    //       there's a result
    //   - Present in state complete:
    //     - Store off the shared_ptr to the DNS result in the entry
    //     - Release the lock
    //     - Return the shared_ptr
    while (retry_cache_lookup)
    {
      if (kmi == _cache.end())
      {
        TRC_DEBUG("Entry not in cache, so create new entry");
        Entry& entry = _cache[key];
        entry.state = Entry::PENDING;
        entry.expiry_i = _expiry_list.end();

        pthread_mutex_unlock(&_lock);
        data_ptr = _factory->get(key, ttl, trail);
        pthread_mutex_lock(&_lock);

        populate_cache_entry(entry, key, ttl, data_ptr);
        retry_cache_lookup = false;

        pthread_cond_broadcast(&_cond);
      }
      else
      {
        TRC_DEBUG("Found the entry in the cache");
        Entry& entry = kmi->second;

        if (entry.state == Entry::PENDING)
        {
          TRC_DEBUG("Cache entry pending, so wait for the factory to complete");
          pthread_cond_wait(&_cond, &_lock);
        }
        else
        {
          TRC_DEBUG("Cache entry is complete, returning now");
          retry_cache_lookup = false;
          data_ptr = entry.data_ptr;
        }
      }
    }

    pthread_mutex_unlock(&_lock);

    return data_ptr;
  }

  /// Check whether an item exists in the cache.
  bool exists(K key)
  {
    bool rc = false;
    pthread_mutex_lock(&_lock);

    // Evict any old entries.
    evict();

    KeyMapIterator i = _cache.find(key);

    if (i != _cache.end())
    {
      rc = true;
    }

    pthread_mutex_unlock(&_lock);

    return rc;
  }

  /// Returns the TTL of an item in the cache.  Returns zero if the item isn't
  /// in the cache at all.
  int ttl(K key)
  {
    int ttl = 0;
    pthread_mutex_lock(&_lock);

    // Evict any old entries.
    evict();

    KeyMapIterator i = _cache.find(key);

    if (i != _cache.end())
    {
      Entry& entry = i->second;
      if (entry.expiry_i != _expiry_list.end())
      {
        ttl = entry.expiry_i->first - time(NULL);
      }
    }

    pthread_mutex_unlock(&_lock);

    return ttl;
  }

private:

  void evict()
  {
    time_t now = time(NULL);

    while ((!_expiry_list.empty()) && (_expiry_list.begin()->first <= now))
    {
      TRC_DEBUG("Current time is %d, expiry time of the entry at the head of "
                "the expiry list is %d",
                now, _expiry_list.begin()->first);

      ExpiryIterator i = _expiry_list.begin();
      KeyMapIterator j = _cache.find(i->second);

      if (j != _cache.end())
      {
        // Set the expiry iterator to _expiry_list.end() as we are about to
        // remove it from the expiry list.
        Entry& entry = j->second;
        entry.expiry_i = _expiry_list.end();

        // Erasing the entry in the cache deletes this instance of the shared
        // pointer. This means new calls of get to the cache will get an
        // up-to-date version of V, but old calls won't have their shared
        // pointers overwritten until they've all finished with them.
        _cache.erase(j);
      }
      _expiry_list.erase(i);

    }
  }

  // Populate the cache entry with the DNS record. There's a chance that the
  // record has been deleted, so we might need to recreate the entry in the
  // cache.
  void populate_cache_entry(Entry& entry,
                            K key,
                            int ttl,
                            std::shared_ptr<V> data_ptr)
  {
    KeyMapIterator kmi = _cache.find(key);

    if (kmi == _cache.end())
    {
      entry = _cache[key];
    }
    else
    {
      entry = kmi->second;
    }

    entry.state = Entry::COMPLETE;

    // Add the entry to the expiry list.
    TRC_DEBUG("Adding entry to expiry list, TTL=%d, expiry time = %d",
              ttl,
              ttl + time(NULL));

    entry.expiry_i = _expiry_list.insert(std::make_pair(ttl + time(NULL), key));
    entry.data_ptr = data_ptr;
  }

  /// Factory object used to get cache data.
  CacheFactory<K, V>* _factory;

  /// Lock protecting the global structures in the cache. This lock must be
  /// held when accessing the global expiry list, key map structures, or cache
  /// entries. It must not be held when calling a factory get() method, but can
  /// be held when calling an evict() method as these are assumed not to block.
  pthread_mutex_t _lock;
  pthread_cond_t _cond;

  ExpiryList _expiry_list;

  KeyMap _cache;
};
#endif
