/**
 * @file memcachedstoreview.cpp Class tracking current view of memcached server cluster
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
#include "cpp_common_pd_definitions.h"


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

void MemcachedStoreView::generate_ring_from_stable_servers()
{
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
        _read_set[ii].push_back(_servers[idx]);
      }
      _write_set[ii] = _read_set[ii];

      // There is no resize in progress, so the current replicas is the same as
      // the read set.
      _current_replicas[ii] = _read_set[ii];
    }

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
    TRC_DEBUG("View is stable with %d nodes", config.servers.size());
    CL_MEMCACHED_CLUSTER_UPDATE_STABLE.log(config.servers.size(),
                                           config.filename.c_str());
    _servers = config.servers;
    generate_ring_from_stable_servers();
  }
  else if (config.servers.empty())
  {
    // Stable configuration.
    TRC_DEBUG("Cluster is moving from 0 nodes to %d nodes", config.new_servers.size());
    CL_MEMCACHED_CLUSTER_UPDATE_RESIZE.log(0,
                                           config.new_servers.size(),
                                           config.filename.c_str());
    _servers = config.new_servers;
    generate_ring_from_stable_servers();
  }
  else
  {
    TRC_DEBUG("Cluster is moving from %d nodes to %d nodes",
              config.servers.size(),
              config.new_servers.size());
    CL_MEMCACHED_CLUSTER_UPDATE_RESIZE.log(config.servers.size(),
                                           config.new_servers.size(),
                                           config.filename.c_str());

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

      // The read and write replicas both consist of all the current primary,
      // then all the new replicas, then the rest of the current replicas
      // (though we don't insert a replica twice).
      //
      // (It would be simpler to just use [current replicas] + [new replicas]
      // but this is not backwards compatible. We used to use a write set of
      // just [current primary] + [new replicas], so we can't put the current
      // backups before the new replicas).
      //
      // This means that we do up to twice as many writes when scaling up/down.
      // This isn't really an issue because scaling is fast now that we have
      // Astaire.
      std::string server = current_nodes[0];
      _read_set[ii].push_back(server);
      _write_set[ii].push_back(server);
      in_set[server] = true;

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

      for (int jj = 1; jj < _replicas; ++jj)
      {
        std::string server = current_nodes[jj];
        if (!in_set[server])
        {
          _read_set[ii].push_back(server);
          _write_set[ii].push_back(server);
          in_set[server] = true;
        }
      }

    }
  }

  if (!(config.servers.empty() && config.new_servers.empty()))
  {
    TRC_DEBUG("New view -\n%s", view_to_string().c_str());
  }
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
  if (!replicas.empty())
  {
    for (size_t ii = 0; ii < replicas.size()-1; ++ii)
    {
      s += replicas[ii] + "/";
    }
    s += replicas[replicas.size()-1];
  }
  return s;
}


/// Constructs a ring used to assign vbuckets to nodes.
MemcachedStoreView::Ring::Ring(int slots) :
  _slots(slots),
  _nodes(0),
  _ring(slots),
  _node_slots()
{
  TRC_DEBUG("Initializing ring with %d slots", slots);
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
  TRC_DEBUG("Updating ring from %d to %d nodes", _nodes, nodes);

  _node_slots.resize(nodes);

  if ((_nodes == 0) && (nodes > 0))
  {
    // Set up the initial ring for the one node case.
    TRC_DEBUG("Set up ring for node 0");
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

  TRC_DEBUG("Completed updating ring, now contains %d nodes", _nodes);
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
        // LCOV_EXCL_START
        // Found the node in the list, so break out and move to the next one.
        unique = false;
        break;
        // LCOV_EXCL_STOP
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

