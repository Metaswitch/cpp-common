/**
 * @file weightedselector.h  Declaration of base class for DNS resolution.
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

#ifndef WEIGHTEDSELECTOR_H__
#define WEIGHTEDSELECTOR_H__

#include <vector>

#include "utils.h"

/// The WeightedSelector class is used to implement resource
/// selection between a number of different options at a single priority
/// level according to the weighting of each record.
/// T is a class with a visible member weight.
template <class T>
class WeightedSelector
{
public:
  /// Constructor.
  WeightedSelector(const std::vector<T>& srvs);

  /// Destructor.
  ~WeightedSelector();

  /// Renders the current state of the tree as a string.
  std::string to_string() const;

  /// Selects an entry and sets its weight to zero.
  int select();

  /// Returns the current total weight of the items in the selector.
  int total_weight();

  // function to generate a random number.  Implememted separately
  // to allow mocking in tests.
  virtual int get_rand();

private:
  std::vector<int> _tree;
};

// We have to declare the functions inline in the header, as this is
// a template class
template <class T>
WeightedSelector<T>::WeightedSelector(const std::vector<T>& srvs) :
  _tree(srvs.size())
{
  // Copy the weights to the tree.
  for (size_t ii = 0; ii < srvs.size(); ++ii)
  {
    _tree[ii] = srvs[ii].get_weight();
  }

  // Work backwards up the tree accumulating the weights.
  for (size_t ii = _tree.size() - 1; ii >= 1; --ii)
  {
    _tree[(ii - 1)/2] += _tree[ii];
  }
}

template <class T>
WeightedSelector<T>::~WeightedSelector()
{
}

template <class T>
int WeightedSelector<T>::select()
{
  // Search the tree to find the item with the smallest cumulative weight that
  // is greater than a random number between zero and the total weight of the
  // tree.
  int s = get_rand();
  size_t ii = 0;

  while (true)
  {
    // Find the left and right children using the usual tree => array mappings.
    size_t l = 2*ii + 1;
    size_t r = 2*ii + 2;

    if ((l < _tree.size()) && (s < _tree[l]))
    {
      // Selection is somewhere in left subtree.
      ii = l;
    }
    else if ((r < _tree.size()) && (s >= _tree[ii] - _tree[r]))
    {
      // Selection is somewhere in right subtree.
      s -= (_tree[ii] - _tree[r]);
      ii = r;
    }
    else
    {
      // Found the selection.
      break;
    }
  }

  // Calculate the weight of the selected entry by subtracting the weight of
  // its left and right subtrees.
  int weight = _tree[ii] -
               (((2*ii + 1) < _tree.size()) ? _tree[2*ii + 1] : 0) -
               (((2*ii + 2) < _tree.size()) ? _tree[2*ii + 2] : 0);

  // Update the tree to set the weight of the selection to zero so it isn't
  // selected again.
  _tree[ii] -= weight;
  int p = ii;
  while (p > 0)
  {
    p = (p - 1)/2;
    _tree[p] -= weight;
  }

  return ii;
}

template <class T>
int WeightedSelector<T>::total_weight()
{
  return _tree[0];
}

template <class T>
int WeightedSelector<T>::get_rand()
{
  int s = rand() % _tree[0];
  return s;
}


#endif
