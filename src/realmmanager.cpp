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

const int RealmManager::DEFAULT_BLACKLIST_DURATION = 30;

RealmManager::RealmManager(Diameter::Stack* stack,
                           std::string realm,
                           int max_peers,
                           DiameterResolver* resolver) :
                           _stack(stack),
                           _realm(realm),
                           _max_peers(max_peers),
                           _resolver(resolver)
{
  pthread_create(&_thread, NULL, thread_function, this);
}

RealmManager::~RealmManager()
{
}

void RealmManager::connection_succeeded(Diameter::Peer* peer)
{
  LOG_INFO("Connected to peer %s", peer->host().c_str());
}

void RealmManager::connection_failed(Diameter::Peer* peer)
{
  LOG_ERROR("Failed to connect to peer %s", peer->host().c_str());
  _peers.erase(std::remove(_peers.begin(), _peers.end(), peer), _peers.end());
  _resolver->blacklist(peer->addr_info(), DEFAULT_BLACKLIST_DURATION);
  // TODO: Signal condition variable to wake up the thread (which will then
  // pick a different host to connect to.
  pthread_cond_signal(&_cond);
}

void* RealmManager::thread_function(void* realm_manager_ptr)
{
  ((RealmManager*)realm_manager_ptr)->thread_function();
  return NULL;
}

void RealmManager::thread_function()
{
  std::vector<AddrInfo> targets;
  _resolver->resolve(_realm, "", _max_peers, targets);

  pthread_mutex_lock(&_lock);

  for (std::vector<AddrInfo>::iterator i = targets.begin();
       i != targets.end();
       i++)
  {
    Diameter::Peer* peer = new Diameter::Peer(*i, "", _realm, 0, this);
    _peers.push_back(peer);
    _stack->add(peer);
  }

  pthread_cond_wait(&_cond, &_lock);
  
}
