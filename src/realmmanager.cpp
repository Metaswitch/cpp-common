/**
 * @file realmmanager.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "realmmanager.h"
#include "utils.h"
#include "cpp_common_pd_definitions.h"

#include <boost/algorithm/string/replace.hpp>

RealmManager::RealmManager(Diameter::Stack* stack,
                           std::string realm,
                           std::string host,
                           int max_peers,
                           DiameterResolver* resolver,
                           Alarm& alarm,
                           const PDLog& peer_comm_restored_log,
                           const PDLog1<const char*>& peer_comm_error_log) :
                           _stack(stack),
                           _realm(realm),
                           _host(host),
                           _max_peers(max_peers),
                           _resolver(resolver),
                           _peer_connection_alarm(alarm),
                           _peer_comm_restored_log(peer_comm_restored_log),
                           _peer_comm_error_log(peer_comm_error_log),
                           _terminating(false)
{
  pthread_mutex_init(&_main_thread_lock, NULL);
  pthread_condattr_t cond_attr;
  pthread_condattr_init(&cond_attr);
  pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
  pthread_cond_init(&_cond, &cond_attr);
  pthread_condattr_destroy(&cond_attr);

  pthread_rwlock_init(&_peers_lock, NULL);
}

void RealmManager::start()
{
  using namespace std::placeholders;

  pthread_create(&_thread, NULL, thread_function, this);
  _stack->register_peer_hook_hdlr("realmmanager",
                                  std::bind(&RealmManager::peer_connection_cb,
                                            this,
                                            _1,
                                            _2,
                                            _3));
  _stack->register_rt_out_cb("realmmanager",
                             std::bind(&RealmManager::srv_priority_cb,
                                       this,
                                       _1));
}

RealmManager::~RealmManager()
{
  pthread_mutex_destroy(&_main_thread_lock);
  pthread_cond_destroy(&_cond);

  pthread_rwlock_destroy(&_peers_lock);
}

void RealmManager::stop()
{
  pthread_mutex_lock(&_main_thread_lock);
  _terminating = true;
  pthread_cond_signal(&_cond);
  pthread_mutex_unlock(&_main_thread_lock);
  pthread_join(_thread, NULL);
  _stack->unregister_peer_hook_hdlr("realmmanager");
  _stack->unregister_rt_out_cb("realmmanager");
}

std::string RealmManager::create_failed_peers_string()
{
  std::string failed_peers_string = "";
  std::string sep = "";

  for (std::pair<AddrInfo, unsigned long> failed_peer : _failed_peers)
  {
    failed_peers_string += sep + Utils::ip_addr_to_arpa(failed_peer.first.address);
    sep = ", ";
  }

  return failed_peers_string;
}

// This function adds a new peer to the _failed_peers map or, if the peer is
// already failed, updates the peer in the map. Returns false if the peer was
// in the map already, otherwise returns true.
bool RealmManager::add_to_failed_peers(Diameter::Peer* peer)
{
  if (_failed_peers.find(peer->addr_info()) == _failed_peers.end())
  {
    // Peer wansn't failed before so add it to _failed_peers
    _failed_peers.insert(std::pair<AddrInfo, unsigned long>(
      peer->addr_info(),
      Utils::current_time_ms()));
    return true;
  }
  else
  {
    // Peer was already failed, update the timestamp
    _failed_peers.find(peer->addr_info())->second = Utils::current_time_ms();
    return false;
  }
}

// This function ensures that the peer is not in _failed_peers map, removing
// it if necessary. It returns whether the peer was in the map at the start.
bool RealmManager::remove_from_failed_peers(Diameter::Peer* peer)
{
  if (_failed_peers.find(peer->addr_info()) == _failed_peers.end())
  {
    // Peer wasn't failed before so nothing to remove
    return false;
  }
  else
  {
    // Peer was failed so remove it
    _failed_peers.erase(_failed_peers.find(peer->addr_info()));
    return true;
  }
}

void RealmManager::remove_old_failed_peers()
{
  unsigned long current_time = Utils::current_time_ms();

  for (std::map<AddrInfo, unsigned long>::iterator ii = _failed_peers.begin();
                                                   ii != _failed_peers.end();)
  {
    if (ii->second + FAILED_PEERS_TIMEOUT_MS <= current_time)
    {
      _failed_peers.erase(ii++);
    }
    else 
    {
      ++ii;
    }
  }
}

// This function is responsible for monitoring diameter peer connections.
//
// The _peer_connection_alarm is used to monitor the connection to diameter
// peers. The parameter controlling the mechanics of the alarm is _max_peers.
// We keep track of the peers we failed to connect to using the map
// _failed_peers. We add the failed peer together with a timestamp to this
// map, so that we can remove stale failed peers which no longer get returned
// by the DNS (see Step 8 in manage_connections).
// We raise the alarm whenever we get a callback and the number of connected
// peers (num_connected_peers) is strictly less than _max_peers.
// We clear the alarm whenever we get a callback and, either the number of
// connected peers is at least _max_peers, or there are no failed peers.
// This ensures we clear the alarm if the total number of available peers is
// less than _max_peers and we successfully connect to all of them.
// We also make ENT logs specifying the ip addresses of the failed peers,
// thus helping us diagnose which peers we failed to connect to.
void RealmManager::monitor_connections(const bool& failed_peers_changed)
{
  // First find the number of connected peers.
  int num_connected_peers = 0;
  for (std::pair<std::string, Diameter::Peer*> peer : _peers)
  {
    if ((peer.second)->connected())
    {
      num_connected_peers++;
    }
  }

  const bool clear_condition = (num_connected_peers >= _max_peers) ||
                                _failed_peers.empty();

  if (_peer_connection_alarm.get_alarm_state() == AlarmState::ALARMED)
  {
    // Alarm is raised so consider clearing it
    if (clear_condition)
    {
      _peer_connection_alarm.clear();
      _peer_comm_restored_log.log();
    }
    else if (failed_peers_changed)
    {
      // We either failed to connect to a new peer or we successfully connected
      // to a previously uncontactable peer. Make an ENT log updating the ip
      // addresses of the uncontactable peers.
      _peer_comm_error_log.log(create_failed_peers_string().c_str());
    }
  }
  else
  {
    // Alarm is not raised so consider raising it
    if (!clear_condition)
    {
      _peer_connection_alarm.set();
      _peer_comm_error_log.log(create_failed_peers_string().c_str());
    }
  }
}

void RealmManager::peer_connection_cb(bool connection_success,
                                      const std::string& host,
                                      const std::string& realm)
{
  pthread_mutex_lock(&_main_thread_lock);
  pthread_rwlock_rdlock(&_peers_lock);

  std::map<std::string, Diameter::Peer*>::iterator ii = _peers.find(host);
  if (ii != _peers.end())
  {
    Diameter::Peer* peer = ii->second;
    if (connection_success)
    {
      if (peer->realm().empty() || (peer->realm().compare(realm) == 0))
      {
        TRC_INFO("Successfully connected to %s in realm %s",
                 host.c_str(),
                 realm.c_str());
        pthread_rwlock_unlock(&_peers_lock);
        pthread_rwlock_wrlock(&_peers_lock);
        bool was_already_failed = remove_from_failed_peers(peer);
        peer->set_connected();

        monitor_connections(was_already_failed);
      }
      else
      {
        TRC_ERROR("Connected to %s in wrong realm (expected %s, got %s), disconnect",
                  host.c_str(),
                  peer->realm().c_str(),
                  realm.c_str());

        _stack->remove(peer);
        _resolver->blacklist(peer->addr_info());
        pthread_rwlock_unlock(&_peers_lock);
        pthread_rwlock_wrlock(&_peers_lock);
        bool wasnt_already_failed = add_to_failed_peers(peer);
        delete peer;
        _peers.erase(ii);

        monitor_connections(wasnt_already_failed);

        pthread_cond_signal(&_cond);
      }
    }
    else
    {
      TRC_ERROR("Failed to connect to %s", host.c_str());
      _resolver->blacklist(peer->addr_info());

      pthread_rwlock_unlock(&_peers_lock);
      pthread_rwlock_wrlock(&_peers_lock);
      bool wasnt_already_failed = add_to_failed_peers(peer);
      delete peer;
      _peers.erase(ii);

      monitor_connections(wasnt_already_failed);

      pthread_cond_signal(&_cond);
    }
  }
  else
  {
    TRC_ERROR("Unexpected host on peer connection callback from freeDiameter: %s",
              host.c_str());
  }

  pthread_rwlock_unlock(&_peers_lock);
  pthread_mutex_unlock(&_main_thread_lock);
  return;
}

void RealmManager::srv_priority_cb(struct fd_list* candidates)
{
  pthread_rwlock_rdlock(&_peers_lock);

  // Run through the list of candidates adjusting their score based on their SRV
  // priority.
  for (struct fd_list* li = candidates->next; li != candidates; li = li->next)
  {
    struct rtd_candidate* candidate = (struct rtd_candidate*)li;
    std::map<std::string, Diameter::Peer*>::iterator ii =
                                             _peers.find(candidate->cfg_diamid);
    if (ii != _peers.end())
    {
      // The lower the priority value, the higher the priority of the result, so
      // take away the priority value from the score.
      if (candidate->score > 0)
      {
        int new_score = candidate->score - (ii->second)->addr_info().priority;

        // Very high priority values shouldn't cause us to go negative - we'll be ignored by
        // freeDiameter
        new_score = std::max(new_score, 1);
        TRC_DEBUG("freeDiameter routing score for candidate %.*s is changing from %d to %d",
                  candidate->cfg_diamidlen,
                  candidate->cfg_diamid,
                  candidate->score,
                  new_score);
        candidate->score = new_score;
      }
      else
      {
        TRC_DEBUG("freeDiameter routing score for candidate %.*s is negative (%d) - not changing",
                  candidate->cfg_diamidlen,
                  candidate->cfg_diamid,
                  candidate->score);
      }
    }
    else
    {
      TRC_WARNING("Unexpected candidate peer %s for Diameter message routing",
                  candidate->cfg_diamid);
    }
  }

  pthread_rwlock_unlock(&_peers_lock);
  return;
}

void* RealmManager::thread_function(void* realm_manager_ptr)
{
  ((RealmManager*)realm_manager_ptr)->thread_function();
  return NULL;
}

// There is a thread running in this function the whole time that
// the program is running. It calls into a function that is responsible
// for managing Diameter connections every time the thread is woken up.
void RealmManager::thread_function()
{
  int ttl = 0;
  struct timespec ts;

  pthread_mutex_lock(&_main_thread_lock);

  do
  {
    manage_connections(ttl);

    // Call pthread_cond_timedwait to pause the thread until either the
    // TTL of one of the DNS entries expires, we get called by a
    // failure to connect to one of the peers, or the program is
    // terminating. In the latter case we exit the loop and tidy up the
    // connections.
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec += ttl;
    pthread_cond_timedwait(&_cond, &_main_thread_lock, &ts);

  } while (!_terminating);

  // Terminating so remove all peers and tidy up.
  for (std::map<std::string, Diameter::Peer*>::iterator ii = _peers.begin();
       ii != _peers.end();
       ii++)
  {
    _stack->remove(ii->second);
    delete (ii->second);
  }

  // This _peers map contains rubbish at this point, don't use it.
  _peers.clear();

  pthread_mutex_unlock(&_main_thread_lock);
}

// This function is responsible for managing Diameter connections to a
// given realm.
//
// We maintain several lists in this function. Here we describe the
// flows in this function and how these lists are used. These can be
// cross referenced with the numbered comments below.
//
// 1. Call resolve to perform a DNS resolution on the given realm and
//    populate targets with a list of potential peers to connect to. This
//    also returns a minimum TTL for the DNS entries.
// 2. Create a list of hostnames from the list of targets. This is
//    new_peers and it is used to compare hostnames with existing
//    connections.
// 3. _peers contains a list of the Diameter::Peer objects which
//    represent peers that we have either connected to, or which
//    we are currently trying to connect to. Filter this list on
//    existing connections into connected_peers. Note that we don't edit _peers
//    directly at this point. Instead we create a local copy of it and save it
//    off in step 7. This is so that we minimise the amount of time we're
//    holding the write lock on it.
// 4. If we have too many connected_peers (i.e. more than _max_peers),
//    remove some connections. We only remove connections to peers
//    that haven't been returned by the resolve function on this run
//    through.
// 5. Connect to any peers in the list of targets that we aren't already
//    connected to. This is how we can end up with too many connections
//    (as per step 4). We don't tear down any connections until we're sure
//    we have _max_peers connections. The only exception to this is
//    when resolve contains fewer than _max_peers entries which means we
//    sure we should have fewer than _max_peers connections.
// 6. Tell the stack the number of peers we are aware of, and the number of
//    peers we're connected to. This is so that the stack can raise appropriate
//    logs when we have no connections and routing messages inevitably fails.
// 7. Update _peers.
// 8. _failed_peers contains diameter peers that we failed to connect to. 
//    We remove entries that are more than a day old to prevent reporting 
//    them as failed once they're no longer being returned by DNS.
//
// On the first run through this function, a lot of this processing is
// irrelevant since we just get a list of targets and try to connect to
// them.
void RealmManager::manage_connections(int& ttl)
{
  std::vector<AddrInfo> targets;
  std::vector<std::string> new_peers;
  std::vector<Diameter::Peer*> connected_peers;
  bool ret;

  // Save a local copy of the current list of peers. We want other functions to
  // still be able to read this list, which is why we don't take the write lock
  // yet. We know that nobody else is editing _peers because we're protected by
  // the _main_thread_lock.
  pthread_rwlock_rdlock(&_peers_lock);
  std::map<std::string, Diameter::Peer*> locked_peers = _peers;
  pthread_rwlock_unlock(&_peers_lock);

  // 1.
  _resolver->resolve(_realm, _host, _max_peers, targets, ttl);

  // We impose sensible max and min values for the TTL.
  ttl = std::max(5, ttl);
  ttl = std::min(300, ttl);

  // 2.
  for (std::vector<AddrInfo>::iterator ii = targets.begin();
       ii != targets.end();
       ii++)
  {
    new_peers.push_back(Utils::ip_addr_to_arpa((*ii).address));
  }

  // 3.
  for (std::map<std::string, Diameter::Peer*>::iterator ii = locked_peers.begin();
       ii != locked_peers.end();
       ii++)
  {
    if ((ii->second)->connected())
    {
      connected_peers.push_back(ii->second);
    }
  }

  // 4.
  for (std::vector<Diameter::Peer*>::iterator ii = connected_peers.begin();
       (ii != connected_peers.end()) &&
       (((int)connected_peers.size() > _max_peers) ||
        ((int)new_peers.size() < _max_peers));
      )
  {
    if (std::find(new_peers.begin(),
                  new_peers.end(),
                  (*ii)->host()) == new_peers.end())
    {
      Diameter::Peer* peer = *ii;
      TRC_STATUS("Removing peer: %s", peer->host().c_str());
      ii = connected_peers.erase(ii);
      std::map<std::string, Diameter::Peer*>::iterator jj =
                                                locked_peers.find(peer->host());

      if (jj != locked_peers.end())
      {
        locked_peers.erase(jj);
      }
      _stack->remove(peer);
      delete peer;
    }
    else
    {
      ii++;
    }
  }

  // 5.
  int zombies = 0;
  for (std::vector<AddrInfo>::iterator ii = targets.begin();
       ii != targets.end();
       ii++)
  {
    std::string hostname = Utils::ip_addr_to_arpa((*ii).address);

    // Check whether this new target is already in our list of peers, and if it
    // isn't, add it.
    std::map<std::string, Diameter::Peer*>::iterator jj =
                                                    locked_peers.find(hostname);
    if (jj == locked_peers.end())
    {
      Diameter::Peer* peer = new Diameter::Peer(*ii, hostname, _realm, 0);
      TRC_STATUS("Adding peer: %s", hostname.c_str());
      ret = _stack->add(peer);
      if (ret)
      {
        locked_peers[hostname] = peer;
      }
      else
      {
        TRC_STATUS("Peer already exists: %s", hostname.c_str());
        delete peer;

        // If the add failed, it means that, while RealmManager and
        // DiameterStack don't have this peer in their list, it still exists in
        // freeDiameter.  This can happen for "zombie" peers whose garbage
        // collection hasn't happened yet.
        //
        // Increment our "zombie" count.
        zombies++;
      }
    }
    else
    {
      jj->second->set_srv_priority(ii->priority);
    }
  }

  // 6. Tell the stack the number of peers we're currently managing (including
  // any peers we are waiting to be able to add, but can't because of delayed
  // zombie cleanup in freeDiameter), and the number of peers we're actually
  // connected to.
  _stack->peer_count(locked_peers.size() + zombies, connected_peers.size());

  // 7. Update the stored _peers map.
  pthread_rwlock_wrlock(&_peers_lock);
  _peers = locked_peers;

  // 8.
  remove_old_failed_peers();

  pthread_rwlock_unlock(&_peers_lock);
}
