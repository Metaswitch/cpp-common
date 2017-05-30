/**
 * @file memcachedstoreview.h Declarations for MemcachedStoreView class.
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */


#ifndef MEMCACHEDSTOREVIEW_H__
#define MEMCACHEDSTOREVIEW_H__

#include <map>
#include <string>
#include <vector>

#include "memcached_config.h"

/// Tracks the current view of the underlying memcached cluster, including
/// calculating the server list and the replica configurations.
class MemcachedStoreView
{
public:
  MemcachedStoreView(int vbuckets, int replicas);
  ~MemcachedStoreView();

  /// Updates the view based on new configuration.
  void update(const MemcachedConfig& config);

  /// Returns the current server list.
  const std::vector<std::string>& servers() const { return _servers; };

  /// Returns the current read and write replica sets for each vbucket.
  const std::vector<std::string>& read_replicas(int vbucket) const { return _read_set[vbucket]; };
  const std::vector<std::string>& write_replicas(int vbucket) const { return _write_set[vbucket]; };

  /// Calculates the vbucket moves that are currently ongoing.
  ///
  /// The returned object has an entry for each moving vbucket ID, giving the
  /// old replica list and the new one.  vBuckets that are not moving are
  /// skipped in the output (thus, if there's no move ongoing, this map is
  /// empty).
  typedef std::vector<std::string> ReplicaList;
  typedef std::pair<ReplicaList, ReplicaList> ReplicaChange;
  const std::map<int, ReplicaChange>& calculate_vbucket_moves() const
  {
    return _changes;
  }

  /// Calculates the replicas that currently own each vbucket.
  const std::map<int, ReplicaList> current_replicas()
  {
    return _current_replicas;
  }

  /// Calculates the replicas that will own each vbucket after the current
  /// resize is complete. If there is no resize in progress, this returns an
  /// empty map.
  const std::map<int, ReplicaList> new_replicas()
  {
    return _new_replicas;
  }

private:
  /// Converts the view into a string suitable for logging.
  std::string view_to_string();
  void generate_ring_from_stable_servers();

  /// Converts a set of replicas into an ordered string suitable for logging.
  std::string replicas_to_string(const std::vector<std::string>& replicas);

  /// Merge two server lists together, removing duplicates.  This does not
  /// preserve ordering.
  std::vector<std::string> merge_servers(const std::vector<std::string>& list1,
                                         const std::vector<std::string>& list2);

  /// Converts a vector of server indexes into a vector of server names.
  ///
  /// For example given an ids vector of [1, 3] and a name table of ["kermit",
  /// "gonzo", "misspiggy"], this function returns ["kermit", "misspiggy"].
  ///
  /// @param ids          - A vector of replica indexes.
  /// @param lookup_table - Table in which to look up the names.
  ///
  /// @return             - A vector of replica names.
  static std::vector<std::string>
    server_ids_to_names(const std::vector<int>& ids,
                        const std::vector<std::string>& lookup_table);

  /// Calculates the ring used to generate the vbucket configurations.  The
  /// ring essentially maps each vbucket slot to a particular node which is
  /// the primary location for data records whose key hashes to that vbucket.
  /// secondary and subsequent replicas are decided by walking around the ring.
  class Ring
  {
  public:
    Ring(int slots);
    ~Ring();

    // Updates the ring to include the specified number of nodes.
    void update(int nodes);

    // Gets the list of replica nodes for the specified slot in the ring.
    // The nodes are guaranteed to be unique if replicas <= nodes, but
    // not otherwise.
    std::vector<int> get_nodes(int slot, int replicas);

  private:

    // Assigns the slot to the specified node.
    void assign_slot(int slot, int node);

    // Finds the nth slot owned by the node.
    int owned_slot(int node, int number);

    // The number of slots in the ring.
    int _slots;

    // The number of nodes currently assigned slots from the ring.
    int _nodes;

    // This is the master ring.
    std::vector<int> _ring;

    // Tracks which slots in the ring each node is assigned.  Indexing is by
    // node, then an ordered map of the assigned slots.
    std::vector<std::map<int, int> > _node_slots;
  };

  // The number of replicas required normally.  During scale-up/down periods
  // some vbuckets may have more read and/or write replicas to maintain
  // redundancy.
  int _replicas;

  // The number of vbuckets being used.
  int _vbuckets;

  // The full list of servers in the memcached cluster.
  std::vector<std::string> _servers;

  // The read and write replica sets for each vbucket.  The first index is the
  // vbucket number.  In stable configurations the read and write set for each
  // vbucket will be the same and have exactly _replicas entries in each.  In
  // unstable configurations (scale-up/scale-down) additional read and write
  // replicas are enabled to maintain redundancy.
  std::vector<std::vector<std::string> > _read_set;
  std::vector<std::vector<std::string> > _write_set;

  // vBucket allocation changes currently ongoing in the cluster (may be
  // empty).
  std::map<int, ReplicaChange> _changes;

  // A map storing the current replicas for each vbucket.
  std::map<int, ReplicaList> _current_replicas;

  // A map storing the new replicas for each vbucket.
  std::map<int, ReplicaList> _new_replicas;
};

#endif
