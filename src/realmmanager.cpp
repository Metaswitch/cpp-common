/**
 * @file realmmanager.cpp
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

#include "log.h"
#include "realmmanager.h"

#include <arpa/inet.h>

const int RealmManager::DEFAULT_BLACKLIST_DURATION = 30;

RealmManager::RealmManager(Diameter::Stack* stack,
                           std::string realm,
                           int max_peers,
                           DiameterResolver* resolver) :
                           _stack(stack),
                           _realm(realm),
                           _max_peers(max_peers),
                           _resolver(resolver),
                           _terminating(false)
{
  pthread_create(&_thread, NULL, thread_function, this);
}

RealmManager::~RealmManager()
{
  pthread_mutex_lock(&_lock);
  _terminating = true; 
  pthread_cond_signal(&_cond);
  pthread_mutex_unlock(&_lock);
}

void RealmManager::connection_succeeded(Diameter::Peer* peer)
{
  LOG_INFO("Connected to peer %s", peer->host().c_str());
  pthread_mutex_lock(&_lock);
  pthread_cond_signal(&_cond);
  pthread_mutex_unlock(&_lock);
}

void RealmManager::connection_failed(Diameter::Peer* peer)
{
  LOG_ERROR("Failed to connect to peer %s", peer->host().c_str());
  _peers.erase(std::remove(_peers.begin(), _peers.end(), peer), _peers.end());
  _resolver->blacklist(peer->addr_info(), DEFAULT_BLACKLIST_DURATION);
  pthread_mutex_lock(&_lock);
  pthread_cond_signal(&_cond);
  pthread_mutex_unlock(&_lock);
}

void* RealmManager::thread_function(void* realm_manager_ptr)
{
  ((RealmManager*)realm_manager_ptr)->thread_function();
  return NULL;
}

void RealmManager::thread_function()
{
  std::vector<AddrInfo> targets;
  int ttl;
  struct timespec ts;
  _resolver->resolve(_realm, "", _max_peers, targets, ttl);
  std::vector<Diameter::Peer*> new_peers;

  pthread_mutex_lock(&_lock);

  for (std::vector<AddrInfo>::iterator i = targets.begin();
       i != targets.end();
       i++)
  {
    Diameter::Peer* peer = new Diameter::Peer(*i, "", _realm, 0, this);
    _peers.push_back(peer);
    _stack->add(peer);
  }

  while (true)
  {
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += (ttl % 1000) * 1000 * 1000;
    ts.tv_sec += ttl / 1000 + ts.tv_nsec / (1000 * 1000 * 1000);
    ts.tv_nsec = ts.tv_nsec % (1000 * 1000 * 1000);
    pthread_cond_timedwait(&_cond, &_lock, &ts);

    if (_terminating)
    {
      break;
    }

    _resolver->resolve(_realm, "", _max_peers, targets, ttl);

    for (std::vector<AddrInfo>::iterator i = targets.begin();
         i != targets.end();
         i++)
    {
      Diameter::Peer* peer = new Diameter::Peer(*i, *i->address, "", 0, this);
      new_peers.push_back(peer);
    }

    // If we currently have too many connections, remove some.
    for (std::vector<Diameter::Peer*>::reverse_iterator ri = _peers.rbegin();
         (ri != _peers.rend()) && ((int)_peers.size() > _max_peers);
         ++ri)
    {
      if (std::find(new_peers.begin(), new_peers.end(), *ri) == new_peers.end())
      {
        _peers.erase(std::remove(_peers.begin(), _peers.end(), *ri), _peers.end());
        _stack->remove(*ri);
      }
    }
    
    // Now connect to any peers returned by the DNS resolver which we
    // weren't already connected to.
    for (std::vector<Diameter::Peer*>::iterator i = new_peers.begin();
         i != new_peers.end();
         i++)
    {
      if (std::find(_peers.begin(), _peers.end(), *i) == new_peers.end())
      {
        _peers.push_back(*i);
        _stack->add(*i);
      }
    }
  }

  // Terminating so remove all peers
  for (std::vector<Diameter::Peer*>::iterator i = _peers.begin();
       i != _peers.end();
       i++)
  {
    _stack->remove(*i);
  }

  _peers.clear();

  pthread_mutex_unlock(&_lock);
}

std::string RealmManager::ip_addr_to_host(IP46Address ip_addr)
{
  if (ip_addr.af == AF_INET)
  {
    char host[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_addr.addr.ipv6, host, INET_ADDRSTRLEN);
  }
  else if (ip_addr.af == AF_INET6)
  {
    char host[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &ip_addr.addr.ipv6, host, INET6_ADDRSTRLEN);
  }
}
