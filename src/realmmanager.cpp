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
#include <boost/algorithm/string/replace.hpp>

const int RealmManager::DEFAULT_BLACKLIST_DURATION = 30;

RealmManager::RealmManager(Diameter::Stack* stack,
                           std::string host,
                           std::string realm,
                           int max_peers,
                           DiameterResolver* resolver) :
                           _stack(stack),
                           _host(host),
                           _realm(realm),
                           _max_peers(max_peers),
                           _resolver(resolver),
                           _terminating(false)
{
  pthread_create(&_thread, NULL, thread_function, this);
  pthread_mutex_init(&_lock, NULL);
  pthread_condattr_t cond_attr;
  pthread_condattr_init(&cond_attr);
  pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
  pthread_cond_init(&_cond, &cond_attr);
  pthread_condattr_destroy(&cond_attr);
}

RealmManager::~RealmManager()
{
  pthread_mutex_lock(&_lock);
  _terminating = true; 
  pthread_cond_signal(&_cond);
  pthread_mutex_unlock(&_lock);
  pthread_join(_thread, NULL);
  pthread_mutex_destroy(&_lock);
  pthread_cond_destroy(&_cond);
}

void RealmManager::connection_succeeded(Diameter::Peer* peer)
{
  LOG_INFO("Connected to peer %s", peer->host().c_str());
}

void RealmManager::connection_failed(Diameter::Peer* peer)
{
  LOG_ERROR("Failed to connect to peer %s", peer->host().c_str());
  pthread_mutex_lock(&_lock);
  std::vector<Diameter::Peer*>::iterator ii = std::find(_peers.begin(), _peers.end(), peer);
  if (ii != _peers.end())
  {
    _peers.erase(ii);
  }
  _resolver->blacklist(peer->addr_info(), DEFAULT_BLACKLIST_DURATION);
  delete peer;

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
  ttl = std::max(1, ttl);
  std::vector<std::string> new_peers;
  bool ret;

  pthread_mutex_lock(&_lock);

  for (std::vector<AddrInfo>::iterator i = targets.begin();
       i != targets.end();
       i++)
  {
    Diameter::Peer* peer = new Diameter::Peer(*i, ip_addr_to_hostname((*i).address), _realm, 0, this);
    LOG_DEBUG("Adding peer: %s", peer->host().c_str());
    ret = _stack->add(peer);
    if (ret)
    {
      _peers.push_back(peer);
    }
    else
    {
      delete peer;
    }
  }

  while (true)
  {
    // Convert the ttl (in ms) into a timespec structure.
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec += ttl;
    pthread_cond_timedwait(&_cond, &_lock, &ts);

    // We've been signalled because we're terminating.
    if (_terminating)
    {
      break;
    }

    _resolver->resolve(_realm, "", _max_peers, targets, ttl);
    ttl = std::max(1, ttl);

    for (std::vector<AddrInfo>::iterator ii = targets.begin();
         ii != targets.end();
         ii++)
    {
      new_peers.push_back(ip_addr_to_hostname((*ii).address));
    }

    // If we currently have too many connections, remove some. This can be because
    // the resolver hasn't returned enough results this time around.
    for (std::vector<Diameter::Peer*>::iterator ii = _peers.begin();
         (ii != _peers.end()) && (((int)_peers.size() > _max_peers) || ((int)new_peers.size() < _max_peers));
         )
    {
      if (std::find(new_peers.begin(), new_peers.end(), (*ii)->host()) == new_peers.end())
      {
        Diameter::Peer* peer = *ii;
        LOG_DEBUG("Removing peer: %s", peer->host().c_str());
        ii = _peers.erase(ii);
        _stack->remove(peer);
        delete (peer);
      }
      else
      {
        ii++;
      }
    }
    
    // Now connect to any peers returned by the DNS resolver which we
    // weren't already connected to.
    for (std::vector<AddrInfo>::iterator ii = targets.begin();
         ii != targets.end();
         ii++)
    {
      std::string hostname = ip_addr_to_hostname((*ii).address);
      bool found = false;
      for (std::vector<Diameter::Peer*>::iterator jj = _peers.begin();
           jj != _peers.end();
           ++jj)
      {
        if ((*jj)->host() == hostname)
        {
          found = true;
          break;
        }
      }

      if (!found)
      {
        Diameter::Peer* peer = new Diameter::Peer(*ii, hostname, _realm, 0, this);
        LOG_DEBUG("Adding peer: %s", peer->host().c_str());
        ret = _stack->add(peer);
        if (ret)
        {
          _peers.push_back(peer);
        }
        else
        {
          LOG_DEBUG("Peer already exists: %s", peer->host().c_str());
          delete peer;
        }
      }
    }

    new_peers.clear();
  }

  // Terminating so remove all peers
  for (std::vector<Diameter::Peer*>::iterator ii = _peers.begin();
       ii != _peers.end();
       ii++)
  {
    _stack->remove(*ii);
    delete (*ii);
  }
  // This _peers vector contains rubbish at this point, don't use it.
  _peers.clear();

  pthread_mutex_unlock(&_lock);
}

std::string RealmManager::ip_addr_to_hostname(IP46Address ip_addr)
{
  std::string hostname;

  // Use inet_ntop to convert the in_addr/in6_addr structure to a
  // string representation of the address. For IPv4 addresses, this
  // is all we need to do. IPv6 addresses contains colons which are
  // not valid characters in hostnames. Instead, we convert the
  // IPv6 address into it's unique reverse lookup form. For example,
  // 2001:dc8::1 becomes
  // 1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.c.d.0.1.0.0.2.ip6.arpa.
  if (ip_addr.af == AF_INET)
  {
    char ipv4_addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_addr.addr.ipv6, ipv4_addr, INET_ADDRSTRLEN);
    hostname = ipv4_addr;
  }
  else if (ip_addr.af == AF_INET6)
  {
    char ipv6_addr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &ip_addr.addr.ipv6, ipv6_addr, INET6_ADDRSTRLEN);
    std::string ipv6_addr_str(ipv6_addr);

    // Reverse the order of the IPv6 address.
    std::reverse(ipv6_addr_str.begin(), ipv6_addr_str.end());

    // Count the number of : - we need to know this because '0000' gets
    // compressed and we need to know how many times this has
    // happened.
    size_t n = std::count(ipv6_addr_str.begin(), ipv6_addr_str.end(), ':');

    // Extract all the hex digits from the IPv6 address into a temporary string
    // and pad out with 0s as appropriate.
    std::string temp_str;
    while (ipv6_addr_str.find(':') != std::string::npos)
    {
      temp_str.append(ipv6_addr_str.substr(0, ipv6_addr_str.find_first_of(':') - 1));
      while (temp_str.length() % 4 != 0)
      {
        temp_str.push_back('0');
      }
      ipv6_addr_str.erase(0, ipv6_addr_str.find_first_of(':'));

      // If the next character is still : then the compression has happened
      // here.
      if (ipv6_addr_str[0] == ':')
      {
        for (size_t i = 0; i < (8 - n); i++)
        {
          temp_str.append("0000");
        }
        ipv6_addr_str.erase(0, 1);
      }
    }

    // Move into hostname and add a . between each character.
    std::string hostname;
    hostname.push_back(temp_str[0]);
    for (size_t i = 1; i < temp_str.length(); i++)
    {
      hostname.push_back('.');
      hostname.push_back(temp_str[i]);
    }

    // Add the ending.
    hostname.append(".ip6.arpa");
  }

  return hostname;
}
