/**
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2016 Metaswitch Networks Ltd
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

#include "timer_heap.h"

void TimerHeap::insert(Timer* t)
{
  if (t->_heap == this)
  {
    return;
  }

  size_t position = _heap_store.size();

  // Update the timer to record where it is in the heap - this allows us to
  // remove or rebalance it quickly, without searching the whole heap for it
  // (which is an O(n) operation and doesn't scale for large numbers of
  // timers).
  t->_heap = this;
  t->_heap_position = position;

  // Add this timer as the bottom element in the heap, then push it upwards
  // so that it reaches its proper place.
  _heap_store.push_back(t);
  heapify_upwards(position);
}

bool TimerHeap::remove(Timer* t)
{
  if (t->_heap != this)
  {
    return false;
  }

  t->_heap = nullptr;

  size_t index = t->_heap_position;

  // Swap this timer with the bottom element of the heap, then reduce the
  // heap size by one, effectively removing the timer.
  _heap_store[index] = _heap_store.back();
  _heap_store.pop_back();

  // Update the timer to record where it is in the heap - this allows us to
  // remove or rebalance it quickly, without searching the whole heap for it
  // (which is an O(n) operation and doesn't scale for large numbers of
  // timers).
  _heap_store[index]->_heap_position = index;

  // Because we've just replaced this element with the bottom element of the
  // heap, it's almost certainly in the wrong place, so move it upwards or
  // downwards as appropriate until the heap property is restored.
  rebalance(_heap_store[index]);

  return true;
}


void TimerHeap::rebalance(Timer* t)
{
  size_t index = t->_heap_position;

  if (index == 0)
  {
    // This is the root node - it only has children, and no parents, so it
    // can only go downwards
    heapify_downwards(index);
  }
  else
  {
    Timer* parent = _heap_store[parent_of(index)];

    if (t->pops_before(parent))
    {
      // This node should pop before its parent, which violates the heap
      // property, so move it upwards to restore the heap property.
      heapify_upwards(index);
    }
    else
    {
      // This timer doesn't pop before its parent, but it might pop after its
      // children, so call heapify_downwards which will push it downwards if
      // necessary
      heapify_downwards(index);
    }
  }
}

TimerHeap::Timer* TimerHeap::get_next_timer()
{
  if (_heap_store.empty())
  {
    return nullptr;
  }
  else
  {
    return _heap_store[0];
  }
}


void TimerHeap::heapify_upwards(size_t index)
{
  if (index == 0)
  {
    // This node is at the root of the tree and can't move upwards any further.
    return;
  }

  Timer* this_child = _heap_store[index];
  Timer* parent = _heap_store[parent_of(index)];

  if (this_child->pops_before(parent))
  {
    // Heap property violated - swap this node with its parent to restore the
    // heap property.
    swap_with_parent(index);

    // Even now that we've swapped it, this node may still pop before its new
    // parent, so check again.
    heapify_upwards(parent_of(index));
  }

}

void TimerHeap::heapify_downwards(size_t index)
{
  // Retrieve this nodes children, leaving the variables as NULL if we're at
  // the end of the tree.
  Timer* left_child = left_child_of(index) < _heap_store.size() ? 
    _heap_store[left_child_of(index)] : nullptr;
  Timer* right_child = right_child_of(index) < _heap_store.size() ?
    _heap_store[right_child_of(index)] : nullptr;

  // Work out whether either of this node's children pop before it.
  unsigned int first_to_pop = index;

  if (left_child &&
      left_child->pops_before(_heap_store[first_to_pop]))
  {
    first_to_pop = left_child_of(index);
  }

  if (right_child &&
      right_child->pops_before(_heap_store[first_to_pop]))
  {
    first_to_pop = right_child_of(index);
  }

  if (first_to_pop != index)
  {
    // One of this node's children will pop before it, which violates the heap
    // property - swap this node with its smallest child, and then check
    // whether its new children pop before it or not.
    swap_with_parent(first_to_pop);
    heapify_downwards(first_to_pop);
  }
}

void TimerHeap::swap_with_parent(size_t index)
{
  Timer* parent = _heap_store[parent_of(index)];

  _heap_store[parent_of(index)] = _heap_store[index];
  _heap_store[index] = parent;

  _heap_store[parent_of(index)]->_heap_position = parent_of(index);
  _heap_store[index]->_heap_position = index;
}

