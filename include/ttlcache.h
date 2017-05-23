/**
 * @file ttlcache.h  Templated implementation of a TTL cache.
 *
 * Copyright (C) Metaswitch Networks 2017
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

#include "log.h"

/// Factory base class for cache.
template <class K, class V>
class CacheFactory
{
public:
  virtual V get(K key, int& ttl, SAS::TrailId trail) = 0;
  virtual void evict(K key, V value) = 0;
};

/// Templated class implementing a cache of items with a specified TTL.
/// Users of this class can use it in a couple of ways, either inserting
/// entries manually using the add API, or registering a factory when the
/// cache is created which is used to create entries on cache misses.
/// The cache is thread-safe and guarantees that multiple concurrent calls
/// to get for the same key will only result in a single call to the factory.
template <class K, class V>
class TTLCache
{
  /// The expiry list is a multimap indexed on expiry time (in seconds since
  /// epoch).
  typedef std::multimap<time_t, K> ExpiryList;
  typedef typename ExpiryList::iterator ExpiryIterator;

  /// The cache itself is a map indexed on the key, where each entry contains
  /// the value plus various housekeeping fields ...
  /// -   state and lock fields used to ensure that each cache entry is only
  ///     populated once even if multiple threads try to get it at the same
  ///     time
  /// -   a reference count
  /// -   the iterator of the item in the expiry list (or expiry_list.end() if
  ///     it is not yet in the list.
  struct Entry
  {
    enum {PENDING, COMPLETE} state;
    pthread_mutex_t lock;
    int refs;
    ExpiryIterator expiry_i;
    V data;
  };

  typedef std::map<K, Entry> KeyMap;
  typedef typename KeyMap::iterator KeyMapIterator;

public:
  TTLCache(CacheFactory<K, V>* factory) :
    _factory(factory),
    _lock(PTHREAD_MUTEX_INITIALIZER),
    _expiry_list(),
    _cache()
  {
  }

  ~TTLCache()
  {
    if (_factory != NULL)
    {
      // Call evict for every entry in the cache.
      for (KeyMapIterator i = _cache.begin();
           i != _cache.end();
           ++i)
      {
        _factory->evict(i->first, i->second.data);
      }
    }
    pthread_mutex_destroy(&_lock);
  }

  /// Get or create an entry in the cache.
  V get(K key, int& ttl, SAS::TrailId trail)
  {
    pthread_mutex_lock(&_lock);

    // Evict any old entries.
    evict();

    KeyMapIterator i = _cache.find(key);

    if (i == _cache.end())
    {
      if (_factory != NULL)
      {
        // The entry is not in the cache, so create a placeholder.
        TRC_DEBUG("Entry not in cache, so create new entry");
        Entry& entry = _cache[key];
        pthread_mutex_init(&entry.lock, NULL);
        entry.state = Entry::PENDING;
        entry.expiry_i = _expiry_list.end();
        pthread_mutex_lock(&entry.lock);

        // Release the global lock and invoke the factory to populate the
        // cache data.
        pthread_mutex_unlock(&_lock);
        entry.data = _factory->get(key, ttl, trail);

        // Cache data should now be populated, so get the global lock again,
        // and mark the entry as complete.
        pthread_mutex_lock(&_lock);
        entry.state = Entry::COMPLETE;

        // Add the entry to the expiry list, and add one to the reference count
        // for this reference.
        ++entry.refs;
        TRC_DEBUG("Adding entry to expiry list, TTL=%d, expiry time = %d", ttl, ttl + time(NULL));
        entry.expiry_i = _expiry_list.insert(std::make_pair(ttl + time(NULL), key));

        // Unlock the entry, so other threads can read it.
        pthread_mutex_unlock(&entry.lock);

        // Increment the reference count on the entry as we are about to return
        // it to an user.
        ++entry.refs;

        pthread_mutex_unlock(&_lock);

        return entry.data;
      }
      else
      {
        // No entry in the cache, and no factory, so just return an empty value.
        pthread_mutex_unlock(&_lock);
        return V();
      }
    }
    else
    {
      TRC_DEBUG("Found the entry in the cache");
      Entry& entry = i->second;

      // Add a reference to the entry so it doesn't get evicted and destroyed
      // from under our feet.
      ++entry.refs;

      // It's now safe to release the global lock.
      pthread_mutex_unlock(&_lock);

      if (entry.state == Entry::PENDING)
      {
        // This cache entry is still being populated, so release the global
        // lock and block on the entry's lock.
        TRC_DEBUG("Cache entry is pending, so wait for the factory to complete");
        pthread_mutex_lock(&entry.lock);
        TRC_DEBUG("Entry is complete");

        // The entry should now be complete, so release the lock on the entry.
        pthread_mutex_unlock(&entry.lock);
      }

      return entry.data;
    }
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

  /// Add an item to the cache with the specified time to live.
  void add(K key, V value, int ttl)
  {
    pthread_mutex_lock(&_lock);

    // Evict any old entries.
    evict();

    KeyMapIterator i = _cache.find(key);

    if (i == _cache.end())
    {
      // Add the entry to the cache.
      Entry& entry = _cache[key];
      pthread_mutex_init(&entry.lock, NULL);
      entry.data = value;
      entry.state = Entry::COMPLETE;

      // Add the entry to the expiry list, and add one to the reference count
      // for this reference.
      ++entry.refs;
      entry.expiry_i = _expiry_list.insert(std::make_pair(ttl + time(NULL), key));
    }
    else
    {
      // Update the cache entry.
      Entry& entry = i->second;
      if (_factory != NULL)
      {
        _factory->evict(key, entry.data);
      }
      entry.data = value;

      // Move the entry in the expiry list.
      if (entry.expiry_i != _expiry_list.end())
      {
        _expiry_list.erase(entry.expiry_i);
        --entry.refs;
      }
      ++entry.refs;
      entry.expiry_i = _expiry_list.insert(std::make_pair(ttl + time(NULL), key));
    }

    pthread_mutex_unlock(&_lock);
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

  void dec_ref(K key)
  {
    // Remove a reference on the specified entry and evict it if it has
    // timed out and there are no more references.
    pthread_mutex_lock(&_lock);
    KeyMapIterator j = _cache.find(key);

    if (j != _cache.end())
    {
      Entry& entry = j->second;
      if (--entry.refs <= 0)
      {
        // Don't release the global lock around eviction - assumption is it
        // isn't a blocking operation.
        if (_factory != NULL)
        {
          _factory->evict(j->first, entry.data);
        }
        pthread_mutex_destroy(&entry.lock);
        _cache.erase(j);
      }
    }

    pthread_mutex_unlock(&_lock);
  }

private:

  void evict()
  {
    time_t now = time(NULL);
    while ((!_expiry_list.empty()) && (_expiry_list.begin()->first <= now))
    {
      TRC_DEBUG("Time now is %d, expiry time of entry at head of expiry list is %d",
                now, _expiry_list.begin()->first);

      ExpiryIterator i = _expiry_list.begin();
      KeyMapIterator j = _cache.find(i->second);

      if (j != _cache.end())
      {
        // Decrement the reference count as the entry is no longer referenced
        // from the expiry list.  If the reference count is now zero we can
        // evict immediately, otherwise wait for other references to end.
        // Set the expiry iterator to _expiry_list.end() as we are about to
        // remove from the expiry list.
        Entry& entry = j->second;
        entry.expiry_i = _expiry_list.end();
        if (--(entry.refs) <= 0)
        {
          // Don't release the global lock around eviction - assumption is it
          // isn't a blocking operation.
          if (_factory != NULL)
          {
            _factory->evict(j->first, entry.data);
          }
          pthread_mutex_destroy(&entry.lock);
          _cache.erase(j);
        }
      }
      _expiry_list.erase(i);

    }
  }

  /// Factory object used to get and evict cache data.
  CacheFactory<K, V>* _factory;

  /// Lock protecting the global structures in the cache.  This lock must be
  /// held when accessing the global expiry list and key map structures.  It
  /// must not be held when calling a factory get() method, but can be held
  /// when calling an evict() method as these are assumed not to block.
  pthread_mutex_t _lock;

  ExpiryList _expiry_list;

  KeyMap _cache;
};
#endif
