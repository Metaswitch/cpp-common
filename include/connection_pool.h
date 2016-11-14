/**
 * @file connectionpool.h  Declaration of template classes for connection
 * pooling
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2016  Metaswitch Networks Ltd
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

#ifndef CONNECTION_POOL_H__
#define CONNECTION_POOL_H__

#include <map>
#include <deque>
#include <pthread.h>
#include <time.h>

#include "log.h"

// Required as AddrInfo is defined here
#include "utils.h"

template <typename T>
class ConnectionHandle;

/// Abstract struct storing a connection object and associated information. The
/// underlying connection type T is required to be copy- and move- constructable;
/// this condition is satisified by letting T be a pointer to a connection
/// object on the heap.
template <typename T>
struct ConnectionInfo
{
  T conn;

  // An AddrInfo object storing details of the target of the connection
  AddrInfo target;

  // The time in seconds that the connection was last used
  time_t last_used_time_s;

  ConnectionInfo(T conn, AddrInfo target) :
    conn(conn),
    target(target),
    last_used_time_s(0)
  {
  }
};

/// Abstract template class storing a pool of connection objects, in "slots",
/// with each distinct target having its own slot. Each connection is wrapped in
/// a ConnectionInfo, and stored in the pool as a pointer.
///
/// Connections can be retrieved from and replaced in the pool, at the front of
/// the slot. Connections that have gone unused for a while are removed
/// periodically from the back of the slots.
///
/// Retrieved connections are wrapped in ConnectionHandle objects, which, when
/// destroyed, handle returning the connection to the pool.
///
/// This class should be subclassed for a specific connection type, with the
/// subclass responsible for implementing the creation and destruction of the
/// type T connection object when required by the pool.
template <typename T>
class ConnectionPool
{
  // The ConnectionHandle class must call the protected release_connection
  // method of this class.
  friend class ConnectionHandle<T>;

  using Slot = std::deque<ConnectionInfo<T>*>;
  using Pool = std::map<AddrInfo, Slot>;

public:
  ConnectionPool(time_t max_idle_time_s, bool free_on_error = false);

  /// The pool cannot be safely emptied in this destructor, as an implementation
  /// of the destroy_connection method is required to safely destroy type T
  /// connection objects. Subclass destructors should call the
  /// destroy_connection_pool method of this class to ensure the pool is
  /// destroyed correctly.
  virtual ~ConnectionPool() {}

  /// Retrieves a connection for the given target from the pool if it exists,
  /// and creates one otherwise. Returns this connection wrapped in a
  /// ConnectionHandle.
  /// (virtual to allow for testing)
  virtual ConnectionHandle<T> get_connection(AddrInfo target);

protected:
  /// Creates a type T connection for the given target
  virtual T create_connection(AddrInfo target) = 0;

  /// Safely destroys a type T connection with the given target
  virtual void destroy_connection(AddrInfo target, T conn) = 0;

  /// Safely destroys the connection pool, leaving it empty. This method must be
  /// called from the destructor of all subclasses
  void destroy_connection_pool();

  /// Called when releasing a connection from its handle. Returns the connection
  /// to the pool or safely destroys it as specified by the second parameter.
  virtual void release_connection(ConnectionInfo<T>* conn_info,
                                  bool return_to_pool);

private:
  /// Removes one connection that has gone unused for more than the max idle
  /// time, if any such connections exist
  void free_old_connection();

  Pool _conn_pool;
  pthread_mutex_t _conn_pool_lock;
  time_t _max_idle_time_s;

  // Whether one dead connection should trigger cleanup of any others to the
  // same target
  bool _free_on_error;
};

/// Template class storing a connection in a ConnectionInfo object. On
/// destruction of the handle, this class handles correctly returning the
/// connection to the pool that it was drawn from.
template <typename T>
class ConnectionHandle
{
public:
  ConnectionHandle(ConnectionInfo<T>* conn_info_ptr, ConnectionPool<T>* conn_pool);

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

  // Sets the value of the return to pool flag. When true, the connection is
  // returned to the pool on destruction of the handle, and when false, the
  // connection is destroyed on destruction of the handle.
  void set_return_to_pool(bool return_to_pool);

private:
  // A ConnectionInfo containing the connection
  ConnectionInfo<T>* _conn_info_ptr;

  // A pointer to the ConnectionPool that created this object
  ConnectionPool<T>* _conn_pool_ptr;

  // True if the connection should be returned to the pool on destruction of the
  // handle, and false if it should be destroyed. Defaults to true.
  bool _return_to_pool;
};

template<typename T>
ConnectionPool<T>::ConnectionPool(time_t max_idle_time_s, bool free_on_error) :
  _max_idle_time_s(max_idle_time_s),
  _free_on_error(free_on_error)
{
  pthread_mutex_init(&_conn_pool_lock, NULL);
}

template<typename T>
void ConnectionPool<T>::destroy_connection_pool()
{
  pthread_mutex_lock(&_conn_pool_lock);
  // Iterate over the slots in the pool. The typename keyword is required to
  // clarify the type declaration to the compiler.
  for (typename Pool::iterator slot_it = _conn_pool.begin();
       slot_it != _conn_pool.end();
       ++slot_it)
  {
    // Iterate over the ConnectionInfo objects in the current slot. The typename
    // keyword is required to clarify the type declaration to the compiler.
    for (typename Slot::iterator conn_info_it = slot_it->second.begin();
         conn_info_it != slot_it->second.end();
         ++conn_info_it)
    {
      // Safely destroy the connection object contained in the current
      // ConnectionInfo
      destroy_connection((*conn_info_it)->target, (*conn_info_it)->conn);
      // Destroy the current ConnectionInfo
      delete *conn_info_it; *conn_info_it = NULL;
    }
  }
  _conn_pool.clear();
  pthread_mutex_unlock(&_conn_pool_lock);
}

template<typename T>
ConnectionHandle<T> ConnectionPool<T>::get_connection(AddrInfo target)
{
  TRC_DEBUG("Request for connection to IP: %s, port: %d",
            target.address.to_string().c_str(),
            target.port);

  ConnectionInfo<T>* conn_info_ptr;

  pthread_mutex_lock(&_conn_pool_lock);

  typename Pool::iterator slot_it = _conn_pool.find(target);

  if ((slot_it != _conn_pool.end()) && (!slot_it->second.empty()))
  {
    // If there is a connection in the pool for the given AddrInfo, retrieve it
    conn_info_ptr = slot_it->second.front();
    slot_it->second.pop_front();
    TRC_DEBUG("Found existing connection %p in pool", conn_info_ptr);
  }
  else
  {
    // If there is no connection in the pool for the given AddrInfo, create a
    // new one
    TRC_DEBUG("No existing connection in pool, create one");
    conn_info_ptr = new ConnectionInfo<T>(create_connection(target), target);
    TRC_DEBUG("Created new connection %p", conn_info_ptr);
  }

  pthread_mutex_unlock(&_conn_pool_lock);

  return ConnectionHandle<T>(conn_info_ptr, this);
}

template<typename T>
void ConnectionPool<T>::release_connection(ConnectionInfo<T>* conn_info_ptr,
                                           bool return_to_pool)
{
  TRC_DEBUG("Release connection to IP: %s, port: %d %s",
            conn_info_ptr->target.address.to_string().c_str(),
            conn_info_ptr->target.port,
            return_to_pool ? "to pool" : "and destroy");

  if (return_to_pool)
  {
    // Update the last used time of the connection
    conn_info_ptr->last_used_time_s = time(NULL);

    pthread_mutex_lock(&_conn_pool_lock);

    // Put the connection back into the pool.
    _conn_pool[conn_info_ptr->target].push_front(conn_info_ptr);

    pthread_mutex_unlock(&_conn_pool_lock);
  }
  else
  {
    if (_free_on_error)
    {
      // Need to destroy all connections for the same target that are currently
      // in the pool
      pthread_mutex_lock(&_conn_pool_lock);

      typename Pool::iterator slot_it = _conn_pool.find(conn_info_ptr->target);
      if (slot_it != _conn_pool.end())
      {
        TRC_DEBUG("Freeing %d other connections", slot_it->second.size());
        while (!slot_it->second.empty())
        {
          ConnectionInfo<T>* conn_info = slot_it->second.front();
          slot_it->second.pop_front();
          destroy_connection(conn_info->target, conn_info->conn);
          delete conn_info; conn_info = NULL;
        }
      }

      pthread_mutex_unlock(&_conn_pool_lock);
    }

    // Now safely destroy the connection and its associated ConnectionInfo
    // (which isn't in the pool, and hence wasn't destroyed above)
    destroy_connection(conn_info_ptr->target, conn_info_ptr->conn);
    delete conn_info_ptr; conn_info_ptr = NULL;
  }

  free_old_connection();
}

template<typename T>
void ConnectionPool<T>::free_old_connection()
{
  time_t current_time = time(NULL);

  pthread_mutex_lock(&_conn_pool_lock);

  // Iterate over the slots
  for (typename Pool::iterator slot_it = _conn_pool.begin();
       slot_it != _conn_pool.end();
       ++slot_it)
  {
    if (!slot_it->second.empty())
    {
      // Connections are always checked in/out at the front of the slot, so the
      // oldest one is at the back
      ConnectionInfo<T>* oldest_conn_info_ptr = slot_it->second.back();

      if (current_time > oldest_conn_info_ptr->last_used_time_s + _max_idle_time_s)
      {
        if (Log::enabled(Log::DEBUG_LEVEL))
        {
          /// Create strings required for debug logging
          std::string addr_info_str = oldest_conn_info_ptr->target.address_and_port_to_string();
          std::string current_time_str = ctime(&current_time);
          std::string last_used_time_s_str = ctime(&(oldest_conn_info_ptr->last_used_time_s));

          TRC_DEBUG("Free idle connection to target: %s (time now is %s, last used %s)",
                    addr_info_str.c_str(),
                    current_time_str.c_str(),
                    last_used_time_s_str.c_str());
        }

        // Delete the connection as it is too old
        slot_it->second.pop_back();
        destroy_connection(oldest_conn_info_ptr->target, oldest_conn_info_ptr->conn);
        delete oldest_conn_info_ptr; oldest_conn_info_ptr = NULL;

        // Delete the entire slot if it is now empty
        if (slot_it->second.empty())
        {
          _conn_pool.erase(slot_it);
        }

        // We have deleted one old connection, so we stop here
        break;
      }
    }
  }

  pthread_mutex_unlock(&_conn_pool_lock);
}

template <typename T>
ConnectionHandle<T>::ConnectionHandle(ConnectionInfo<T>* conn_info_ptr,
                                      ConnectionPool<T>* conn_pool_ptr) :
  _conn_info_ptr(conn_info_ptr),
  _conn_pool_ptr(conn_pool_ptr),
  _return_to_pool(true)
{
}

template <typename T>
ConnectionHandle<T>::~ConnectionHandle()
{
  // On destruction, release the connection back into the pool, or destroy it.
  // If this object has been moved, the _conn_info_ptr will be null, so this
  // case is checked for.
  if (_conn_info_ptr)
  {
    _conn_pool_ptr->release_connection(_conn_info_ptr, _return_to_pool);
  }
}

template <typename T>
ConnectionHandle<T>::ConnectionHandle(ConnectionHandle<T>&& conn_handle) :
  _conn_info_ptr(conn_handle._conn_info_ptr),
  _conn_pool_ptr(conn_handle._conn_pool_ptr)
{
  conn_handle._conn_info_ptr = NULL;
  conn_handle._conn_pool_ptr = NULL;
}

template <typename T>
ConnectionHandle<T>& ConnectionHandle<T>::operator=(ConnectionHandle<T>&& conn_handle)
{
  _conn_info_ptr = conn_handle._conn_info_ptr; conn_handle._conn_info_ptr = NULL;
  _conn_pool_ptr = conn_handle._conn_pool_ptr; conn_handle._conn_pool_ptr = NULL;
  return *this;
}

template <typename T>
T ConnectionHandle<T>::get_connection()
{
  return _conn_info_ptr->conn;
}

template <typename T>
AddrInfo ConnectionHandle<T>::get_target()
{
  return _conn_info_ptr->target;
}

template <typename T>
void ConnectionHandle<T>::set_return_to_pool(bool return_to_pool)
{
  _return_to_pool = return_to_pool;
}
#endif
