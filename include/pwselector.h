/**
 * @file pwselector.h  Templated implementation of a priority-weighted selector.
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

#ifndef PWSELECTOR_H__
#define PWSELECTOR_H__

#include "log.h"

/// Class implementing an efficient weighted selection algorithm.  Both
/// selection and changing item weights are O(n) operations.
template <class T>
class WSelector
{
  typedef typename std::vector<std::pair<int, T> > SelectionTree;
  typedef typename std::map<T, int> SelectionMap;

public:
  /// Constructs a weighted selector with the specified selections.
  WSelector(std::vector<std::pair<int, T> > selections) :
    _tree(selections),
    _sel2slot()
  {
    // recalculate the weights, by working backwards, adding the weight of
    // each item to its parent weight until we reach the root of the heap.
    // At the end of this loop the weight of each item in the heap should be
    // the total of the weights of its children.
    for (size_t ii = _tree.size() - 1; ii >= 1; --ii)
    {
      _tree[(ii-1)/2].first += _tree[ii].first;
      _sel2slot[_tree[ii].second] = ii;
    }
  }

  /// Destructor
  ~WSelector()
  {
  }

  /// Returns the selection weight of the specified item.
  int weight(T selection) const
  {
    int w = 0;
    typename SelectionMap::const_iterator i = _sel2slot.find(selection);
    if (i != _sel2slot.end())
    {
      w = weight(i->second);
    }

    return w;
  }

  /// Returns the total selection weight of the selector.  This is always the
  /// weight in first entry in the heap.
  int total_weight() const { return _tree[0].first; };

  /// Updates the weight of an item in the selector.
  void set_weight(T selection, int new_weight)
  {
    size_t slot;
    typename SelectionMap::const_iterator i = _sel2slot.find(selection);
    if (i == _sel2slot.end())
    {
      // New entry, so add it to the end of the heap with zero weight.
      slot = _tree.size();
      _tree.push_back(std::make_pair(0, selection));
      _sel2slot[selection] = slot;
    }
    else
    {
      // Existing entry.
      slot = i->second;
    }

    int delta = new_weight - weight(slot);

    if (delta != 0)
    {
      // Update the weight on the specified item and all its ancestors.
      _tree[slot].first += delta;
      while (slot > 0)
      {
        slot = (slot-1)/2;
        _tree[slot].first += delta;
      }
    }
  }

  /// Randomly selects a item according to the current weightings.
  T select()
  {
    // Generate a random number between zero and the cumulative weight of all
    // the items in the heap (which is always the first entry).
    int s = rand() % _tree[0].first;
    LOG_DEBUG("Random number %d (out of total weight %d)", s, _tree[0].first);

    // Now find the item with the smallest cumulative weight that is greater than
    // the random number by searching down the heap.
    size_t i = 0;

    while (true)
    {
      // Find the left and right children using the usual heap => array mappings.
      size_t l = 2*i + 1;
      size_t r = 2*i + 2;

      if ((l < _tree.size()) && (s < _tree[l].first))
      {
        // Selection is somewhere in left subheap.
        i = l;
      }
      else if ((r < _tree.size()) && (s >= _tree[i].first - _tree[r].first))
      {
        // Selection is somewhere in right subheap.
        s -= (_tree[i].first - _tree[r].first);
        i = r;
      }
      else
      {
        // Found the selection.
        break;
      }
    }

    return _tree[i].second;
  }

private:
  /// Returns the weight of a selection at the specified slot in the heap.
  /// This is calculated by subtracting the weight of the children in the heap
  /// from the specified node.
  int weight(size_t slot) const
  {
    int w = 0;
    if (slot < _tree.size())
    {
      w = _tree[slot].first;
      size_t l = 2*slot + 1;
      size_t r = 2*slot + 2;
      if (l < _tree.size())
      {
        w -= _tree[l].first;
      }
      if (r < _tree.size())
      {
        w -= _tree[r].first;
      }
    }

    return w;
  }

  /// The cumulative selection tree.  This is a tree mapped on to a vector
  /// using the standard mappings (that is, left child of node i is at 2i+1 and
  /// right child is at 2i+2).
  SelectionTree _tree;

  /// Map from the selection types to the slot in the heap.
  SelectionMap _sel2slot;
};

/// Templated class implementing efficient priority/weighted selection,
/// including support for dynamic, timed blacklisting of selections.  The
/// selector always chooses an item from the highest priority list with
/// unblacklisted items, and selects from those items according to their
/// weighting.  Both selection and blacklisting are O(n) operations.
template <class T>
class PWSelector
{
  typedef typename std::map<T, int> PriorityMap;
  typedef typename std::multimap<int, std::pair<T, int> > BlackList;

public:
  /// Creates a priority/weighted selector containing the specified
  /// selections.  The outer vector is ordered by decreasing priority.
  PWSelector(std::vector<std::vector<std::pair<int, T> > > selections) :
    _wselectors(),
    _sel2p(),
    _blacklist()
  {
    for (size_t ii = 0; ii < selections.size(); ++ii)
    {
      _wselectors.push_back(WSelector<T>(selections[ii]));
      for (size_t jj = 0; jj < selections[ii].size(); ++jj)
      {
        _sel2p[selections[ii][jj].second] = ii;
      }
    }
  }

  /// Destroys the priority/weighted selector.
  ~PWSelector()
  {
  }

  /// Makes a selection from the highest priority bucket with non-zero
  /// weighted selections.
  T select()
  {
    T selection;

    expire_blacklist();

    for (size_t ii = 0; ii < _wselectors.size(); ++ii)
    {
      LOG_DEBUG("Try selecting from priority level %d, total weight %d", ii, _wselectors[ii].total_weight());
      if (_wselectors[ii].total_weight() > 0)
      {
        selection = _wselectors[ii].select();
        break;
      }
    }
    return selection;
  }

  /// Blacklists the selection for the specified duration (in seconds).
  void blacklist(T selection, int duration)
  {
    // Check that the selection is not already blacklisted.
    typename PriorityMap::const_iterator i = _sel2p.find(selection);

    if (i != _sel2p.end())
    {
      int wgt = _wselectors[i->second].weight(selection);

      if (wgt > 0)
      {
        // selection is not blacklisted, so change weight to zero and add it
        // to the blacklist.
        _wselectors[i->second].set_weight(selection, 0);
        _blacklist.insert(std::make_pair(duration + time(NULL),
                                         std::make_pair(selection, wgt)));
      }
    }
  }

private:

  /// Checks for expired blacklist entries, and reinstates the selections
  /// with their original weights.
  void expire_blacklist()
  {
    int now = time(NULL);

    while ((!_blacklist.empty()) && (_blacklist.begin()->first < now))
    {
      // Blacklist entry has timed out, so switch the weight of the selection
      // back to the old weight.
      typename BlackList::iterator i = _blacklist.begin();
      typename PriorityMap::const_iterator j = _sel2p.find(i->second.first);
      if (j != _sel2p.end())
      {
        _wselectors[j->second].set_weight(i->second.first, i->second.second);
      }
      _blacklist.erase(i);
    }
  }

  /// Vector holding the weighted selectors, in priority order.
  typename std::vector<WSelector<T>> _wselectors;

  /// Map for resolving a particular selection to the containing weighted
  /// selector.
  PriorityMap _sel2p;

  /// Blacklist indexed on the time the blacklisting ends.  Each entry contains
  /// the selection item and the original weight.
  BlackList _blacklist;
};

#endif
