/**
 * @file connectionpool.h  Declaration of template classes for connection
 * pooling
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
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

#ifndef CONNECTIONPOOL_H__
#define CONNECTIONPOOL_H__

#include <map>
#include <deque>
#include <pthread.h>
#include <time.h>

#include "log.h"
#include "baseresolver.h"

template <typename T>
class ConnectionHandle;

/// Abstract struct storing a connection object and associated information
template <typename T>
struct ConnectionInfo
{
  T conn;
  AddrInfo target;
  time_t last_used_time_s;
  ConnectionInfo(T conn, AddrInfo target) :
    conn(conn),
    target(target)
  {
  }
};

/// Abstract template class storing a pool of connection objects, in "slots",
/// with each distinct target having its own slot.
///
/// Connections can be retrieved from and replaced in the pool, at the front of
/// the slot. Connections that have gone unused for a while are removed
/// periodically from the back of the slots.
template <typename T>
class ConnectionPool
{
  // The ConnectionHandle class must call the protected release_connection
  // method of this class.
  friend class ConnectionHandle<T>;

  using Slot = std::deque<ConnectionInfo<T>>;
  using Pool = std::map<AddrInfo, Slot>;

public:
  ConnectionPool(time_t max_idle_time_s);
  virtual ~ConnectionPool() {}

  /// Retrieves a connection for the given target from the pool if it exists,
  /// and creates one otherwise. Returns this connection wrapped in a
  /// ConnectionHandle.
  ConnectionHandle<T> get_connection(AddrInfo target);

protected:
  /// Creates a type T connection for the given target
  virtual T create_connection(AddrInfo target) = 0;
  /// Safely destroys a type T connection
  virtual void destroy_connection(T conn) = 0;
  /// Safely destroys the connection pool. This method must be called from the
  /// destructor of all subclasses
  void destroy_connection_pool();

  /// Releases the given connection back into the pool
  void release_connection(ConnectionInfo<T> conn_info);

private:
  /// Removes one connection that has gone unused for the max idle time, if any
  /// exist
  void free_old_connection();

  Pool _conn_pool;
  pthread_mutex_t _conn_pool_lock;
  time_t _max_idle_time_s;
};

/// Template class storing a connection object. On destruction of the handle,
/// this class handles correctly returning the connection to the pool that it
/// was drawn from.
template <typename T>
class ConnectionHandle
{
public:
  ConnectionHandle(ConnectionInfo<T> conn_info, ConnectionPool<T>* conn_pool);
  // The copy constructor is deleted to avoid having copies of the handle, which
  // could cause the connection to get checked back in twice
  ConnectionHandle(const ConnectionHandle<T>&) = delete;
  ConnectionHandle<T>& operator= (const ConnectionHandle<T>&) = delete;
  // Move constructors
  ConnectionHandle<T>(ConnectionHandle<T>&& conn_handle);
  ConnectionHandle<T>& operator= (ConnectionHandle<T>&&);
  // The destructor handles releasing the connection back into the pool.
  ~ConnectionHandle();

  // Gets the connection object contained within _conn_info
  T get_connection();
  // Gets the AddrInfo object contained within _conn_info
  AddrInfo get_target();

private:
  ConnectionInfo<T> _conn_info;
  // A pointer to the ConnectionPool that created this object
  ConnectionPool<T>* _conn_pool_ptr;
};

template<typename T>
ConnectionPool<T>::ConnectionPool(time_t max_idle_time_s) :
  _max_idle_time_s(max_idle_time_s)
{
  pthread_mutex_init(&_conn_pool_lock, NULL);
}

template<typename T>
void ConnectionPool<T>::destroy_connection_pool()
{
  pthread_mutex_lock(&_conn_pool_lock);
  // Iterate over the slots in the pool
  for (typename Pool::iterator slot = _conn_pool.begin();
       slot != _conn_pool.end();
       ++slot)
  {
    // Iterate over the current slot
    for (typename Slot::iterator conn_info = slot->second.begin();
         conn_info != slot->second.end();
         ++conn_info)
    {
      // Safely destroy the connection object contained in the current
      // ConnectionInfo
      destroy_connection((*conn_info).conn);
    }
  }
  pthread_mutex_unlock(&_conn_pool_lock);
}

template<typename T>
ConnectionHandle<T> ConnectionPool<T>::get_connection(AddrInfo target)
{
  ConnectionInfo<T>* conn_info;

  TRC_DEBUG("Request for connection to IP: %s, port: %d",
            target.address.to_string().c_str(),
            target.port);

  pthread_mutex_lock(&_conn_pool_lock);

  typename Pool::iterator it = _conn_pool.find(target);

  if ((it != _conn_pool.end()) && (!it->second.empty()))
  {
    conn_info = &(it->second.front());
    it->second.pop_front();
    TRC_DEBUG("Found existing connection %p in pool", conn_info);
  }
  else
  {
    TRC_DEBUG("No existing connection in pool, create one");
    conn_info = new ConnectionInfo<T>(create_connection(target), target);
    TRC_DEBUG("Created new connection %p", conn_info);
  }

  pthread_mutex_unlock(&_conn_pool_lock);

  return ConnectionHandle<T>(*conn_info, this);
}

template<typename T>
void ConnectionPool<T>::release_connection(ConnectionInfo<T> conn_info)
{
  TRC_DEBUG("Release connection to IP: %s, port: %d",
            conn_info.target.address.to_string().c_str(),
            conn_info.target.port);

  // Update the last used time of the connection
  conn_info.last_used_time_s = time(NULL);

  pthread_mutex_lock(&_conn_pool_lock);

  // Put the connection back into the pool.
  _conn_pool[conn_info.target].push_front(conn_info);

  pthread_mutex_unlock(&_conn_pool_lock);

  free_old_connection();
}

template<typename T>
void ConnectionPool<T>::free_old_connection()
{
  time_t current_time = time(NULL);

  pthread_mutex_lock(&_conn_pool_lock);

  // Iterate over the slots
  for (typename Pool::iterator slot = _conn_pool.begin();
       slot != _conn_pool.end();
       ++slot)
  {
    if (!slot->second.empty())
    {
      // Connections are always checked in/out at the front of the slot, so the
      // oldest one is at the back
      ConnectionInfo<T> oldest_conn_info = slot->second.back();

      if(current_time > oldest_conn_info.last_used_time_s + _max_idle_time_s)
      {
        TRC_DEBUG("Free idle connection to IP: %s, port: %d (time now is %ld, last used %ld)",
                  oldest_conn_info.target.address.to_string().c_str(),
                  oldest_conn_info.target.port,
                  ctime(&current_time),
                  ctime(&oldest_conn_info.last_used_time_s));

        // Delete the connection as it is too old
        slot->second.pop_back();
        destroy_connection(oldest_conn_info.conn);

        // Delete the entire slot if it is now empty
        if (slot->second.empty())
        {
          _conn_pool.erase(slot);
        }

        // We have deleted one old connection, so we stop here
        break;
      }
    }
  }

  pthread_mutex_unlock(&_conn_pool_lock);
}

template <typename T>
ConnectionHandle<T>::ConnectionHandle(ConnectionInfo<T> conn_info,
                                      ConnectionPool<T>* conn_pool_ptr) :
  _conn_info(conn_info),
  _conn_pool_ptr(conn_pool_ptr)
{
}

template <typename T>
ConnectionHandle<T>::~ConnectionHandle()
{
  (*_conn_pool_ptr).release_connection(_conn_info);
}

template <typename T>
ConnectionHandle<T>::ConnectionHandle(ConnectionHandle<T>&& conn_handle) :
  _conn_info(conn_handle._conn_info),
  _conn_pool_ptr(conn_handle._conn_pool_ptr)
{
}

template <typename T>
ConnectionHandle<T>& ConnectionHandle<T>::operator=(ConnectionHandle<T>&& conn_handle)
{
  _conn_info = conn_handle._conn_info;
  _conn_pool_ptr = conn_handle._conn_pool_ptr;
  return *this;
}

template <typename T>
T ConnectionHandle<T>::get_connection()
{
  return _conn_info.conn;
}

template <typename T>
AddrInfo ConnectionHandle<T>::get_target()
{
  return _conn_info.target;
}

#endif
