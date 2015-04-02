/**
 * @file memcachedstoreview.cpp Class tracking current view of memcached server cluster
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


// Common STL includes.
#include <cassert>
#include <vector>
#include <map>
#include <list>
#include <set>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "log.h"
#include "memcachedstoreview.h"


MemcachedStoreView::MemcachedStoreView(int vbuckets, int replicas) :
  _replicas(replicas),
  _vbuckets(vbuckets),
  _read_set(vbuckets),
  _write_set(vbuckets)
{
}


MemcachedStoreView::~MemcachedStoreView()
{
}

std::vector<std::string> MemcachedStoreView::merge_servers(const std::vector<std::string>& list1,
                                                           const std::vector<std::string>& list2)
{
  std::set<std::string> merged_servers;
  merged_servers.insert(list1.begin(), list1.end());
  merged_servers.insert(list2.begin(), list2.end());

  std::vector<std::string> ret(merged_servers.begin(), merged_servers.end());
  return ret;
}

std::vector<std::string> MemcachedStoreView::
  server_ids_to_names(const std::vector<int>& ids,
                      const std::vector<std::string>& lookup_table)
{
  std::vector<std::string> names;

  for (std::vector<int>::const_iterator it = ids.begin();
       it != ids.end();
       ++it)
  {
    names.push_back(lookup_table[*it]);
  }

  return names;
}

/// Updates the view for new current and target server lists.
void MemcachedStoreView::update(const MemcachedConfig& config)
{
  // Clear out any state from the old view.
  _changes.clear();
  _current_replicas.clear();
  _new_replicas.clear();

  for (int ii = 0; ii < _vbuckets; ++ii)
  {
    _read_set[ii].clear();
    _write_set[ii].clear();
  }

  // Generate the appropriate rings and the resulting vbuckets arrays.
  if (config.new_servers.empty())
  {
    // Stable configuration.
    LOG_DEBUG("View is stable with %d nodes", config.servers.size());
    _servers = config.servers;

    // Only need to generate a single ring.
    Ring ring(_vbuckets);
    ring.update(_servers.size());

    int replicas = _replicas;
    if (replicas > (int)_servers.size())
    {
      // Not enough servers for the required level of replication.
      replicas = _servers.size();
    }

    // Generate the read and write replica sets from the rings.
    for (int ii = 0; ii < _vbuckets; ++ii)
    {
      std::vector<int> server_indexes = ring.get_nodes(ii, replicas);
      for (size_t jj = 0; jj < server_indexes.size(); jj++)
      {
        int idx = server_indexes[jj];
        _read_set[ii].push_back(config.servers[idx]);
      }
      _write_set[ii] = _read_set[ii];

      // There is no resize in progress, so the current replicas is the same as
      // the read set.
      _current_replicas[ii] = _read_set[ii];
    }
  }
  else
  {
    LOG_DEBUG("Cluster is moving from %d nodes to %d nodes",
              config.servers.size(),
              config.new_servers.size());

    // _servers should contain all the servers we might want to store
    // data on, so combine the old and new server lists, removing any overlap.
    _servers = merge_servers(config.servers, config.new_servers);

    // Calculate the two rings needed to generate the vbucket replica sets
    Ring current_ring(_vbuckets);
    current_ring.update(config.servers.size());
    Ring new_ring(_vbuckets);
    new_ring.update(config.new_servers.size());

    for (int ii = 0; ii < _vbuckets; ++ii)
    {
      // Keep track of which nodes are in the replica sets to avoid duplicates.
      std::map<std::string, bool> in_set;

      // Calculate the read and write replica sets for this bucket for both
      // current and target node sets.
      std::vector<std::string> current_nodes =
        server_ids_to_names(current_ring.get_nodes(ii, _replicas),
                            config.servers);
      std::vector<std::string> new_nodes =
        server_ids_to_names(new_ring.get_nodes(ii, _replicas),
                            config.new_servers);

      // Firstly, store off the replicas for this vbucket.
      _current_replicas[ii] = current_nodes;
      _new_replicas[ii] = new_nodes;

      // Determine if the set of nodes has changed by sorting the above two
      // vectors and comparing.
      std::vector<std::string> current_nodes_sorted = current_nodes;
      std::vector<std::string> new_nodes_sorted = new_nodes;
      std::sort(current_nodes_sorted.begin(), current_nodes_sorted.end());
      std::sort(new_nodes_sorted.begin(), new_nodes_sorted.end());

      if (current_nodes_sorted != new_nodes_sorted)
      {
        // Lists are different, add an entry to _changes to indicate this.
        //
        // Build the structure from the inside out (build both replica lists,
        // create a pair from them then add to the _changes map under the
        // vbucket index.
        std::pair<std::vector<std::string>, std::vector<std::string>>
          change_entry(current_nodes, new_nodes);
        _changes[ii] = change_entry;
      }

      // The read and write replicas both consist of all the old replicas,
      // followed by all the new replicas (though we don't insert a replica
      // twice).
      //
      // This means that we do up to twice as many writes when scaling up/down.
      // This isn't really an issue because scaling is fast now that we have
      // Astaire.
      for (int jj = 0; jj < _replicas; ++jj)
      {
        std::string server = current_nodes[jj];
        if (!in_set[server])
        {
          _read_set[ii].push_back(server);
          _write_set[ii].push_back(server);
          in_set[server] = true;
        }
      }

      for (int jj = 0; jj < _replicas; ++jj)
      {
        std::string server = new_nodes[jj];
        if (!in_set[server])
        {
          _read_set[ii].push_back(server);
          _write_set[ii].push_back(server);
          in_set[server] = true;
        }
      }
    }
  }

  LOG_DEBUG("New view -\n%s", view_to_string().c_str());
}


/// Renders the view as a concise string suitable for logging.
std::string MemcachedStoreView::view_to_string()
{
  // Render the view with vbuckets as rows and replicas as columns.
  std::ostringstream oss;

  // Write the header.
  oss << std::left << std::setw(8) << std::setfill(' ') << "Bucket";
  oss << std::left << std::setw(30) << std::setfill(' ') << "Write";
  oss << "Read" << std::endl;
  for (int ii = 0; ii < _vbuckets; ++ii)
  {
    oss << std::left << std::setw(8) << std::setfill(' ') << std::to_string(ii);
    oss << std::left << std::setw(28) << std::setfill(' ') << replicas_to_string(_write_set[ii]);
    oss << "||";
    oss << replicas_to_string(_read_set[ii]) << std::endl;
  }
  return oss.str();
}


std::string MemcachedStoreView::replicas_to_string(const std::vector<std::string>& replicas)
{
  std::string s;
  for (size_t ii = 0; ii < replicas.size()-1; ++ii)
  {
    s += replicas[ii] + "/";
  }
  s += replicas[replicas.size()-1];

  return s;
}


/// Constructs a ring used to assign vbuckets to nodes.
MemcachedStoreView::Ring::Ring(int slots) :
  _slots(slots),
  _nodes(0),
  _ring(slots),
  _node_slots()
{
  LOG_DEBUG("Initializing ring with %d slots", slots);
}


MemcachedStoreView::Ring::~Ring()
{
}


/// Updates the ring to have the specified number of nodes.  This is done
/// incrementally starting from the current number of nodes and reassigning
/// buckets to new nodes one by one.  This algorithm ensures that as the size
/// of the ring increases, slots are either left alone or assigned to new nodes
/// - never assigned to existing nodes.  This property is important for
/// maintaining redundancy as the cluster grows.
///
/// Unfortunately we can't current see how to run the algorithm in reverse,
/// so if the number of nodes reduces the ring must be destroyed and recreated.
void MemcachedStoreView::Ring::update(int nodes)
{
  LOG_DEBUG("Updating ring from %d to %d nodes", _nodes, nodes);

  _node_slots.resize(nodes);

  if (_nodes == 0)
  {
    // Set up the initial ring for the one node case.
    LOG_DEBUG("Set up ring for node 0");
    for (int i = 0; i < _slots; ++i)
    {
      _ring[i] = -1;
      assign_slot(i, 0);
    }
    _nodes = 1;
  }

  while (_nodes < nodes)
  {
    // Increasing the number of nodes, so reassign slots from existing nodes
    // to the next new node.  By choosing the first slot assigned to the
    // most heavily loaded node, then the second slot assigned to the
    // next most heavily loaded, anon.
    int replace_slots = _slots/(_nodes+1);

    for (int i = 0; i < replace_slots; ++i)
    {
      // Find the node which will be replaced, by finding the node with the
      // most assigned slots.  Ties are broken in favour of the highest numbered
      // node.
      int replace_node = 0;
      for (int node = 1; node < _nodes; ++node)
      {
        if (_node_slots[node].size() >= _node_slots[replace_node].size())
        {
          replace_node = node;
        }
      }

      // Now replace the appropriate slot assignment.  (For the first
      int slot = owned_slot(replace_node, i);
      assign_slot(slot, _nodes);
    }

    _nodes += 1;
  }

  LOG_DEBUG("Completed updating ring, now contains %d nodes", _nodes);
}


/// Gets the set of nodes that should be used to store a number of replicas
/// of data where the record key hashes to the appropriate slot in the ring.
/// This is done by starting at the slot, and returning the first n unique
/// nodes walking around the ring.  If there are not enough unique nodes,
/// remaining replica slots are filled with the first node.
std::vector<int> MemcachedStoreView::Ring::get_nodes(int slot, int replicas)
{
  std::vector<int> node_list;
  node_list.reserve(replicas);

  int next_slot = slot;

  while (node_list.size() < (size_t)std::min(replicas, _nodes))
  {
    bool unique = true;

    // Check that the next node in the ring isn't already in the node list.
    for (size_t i = 0; i < node_list.size(); ++i)
    {
      if (node_list[i] == _ring[next_slot])
      {
        // Found the node in the list, so break out and move to the next one.
        unique = false;
        break;
      }
    }

    if (unique)
    {
      // Found a node that is not already in the list, so add it.
      node_list.push_back(_ring[next_slot]);
    }
    next_slot = (next_slot + 1) % _slots;
  }

  while (node_list.size() < (size_t)replicas)
  {
    // Must not be enough nodes for the level of replication requested, so
    // just fill remaining slots with the first node assigned to this slot.
    node_list.push_back(_ring[slot]);
  }

  return node_list;
}


/// Assigns the specified slot to the specified node.  This also keeps the
/// _node_slots maps in sync.
void MemcachedStoreView::Ring::assign_slot(int slot, int node)
{
  int old_node = _ring[slot];
  if (old_node != -1)
  {
    std::map<int, int>::iterator i = _node_slots[old_node].find(slot);
    assert(i != _node_slots[node].end());
    _node_slots[old_node].erase(i);
  }
  _ring[slot] = node;
  _node_slots[node][slot] = slot;
}


/// Returns the nth slot owned by the specified node.  If n is greater
/// than the number of slots owned by the node, return the nth slot modulo
/// the total number of owned slots.
int MemcachedStoreView::Ring::owned_slot(int node, int number)
{
  number = number % _node_slots[node].size();

  std::map<int,int>::const_iterator i;
  int j;
  for (i = _node_slots[node].begin(), j = 0;
       j < number;
       ++i, ++j)
  {
  }

  return i->second;
}

