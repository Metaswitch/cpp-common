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

#ifndef TIMER_HEAP_H
#define TIMER_HEAP_H

#include <stdint.h>
#include <stddef.h>
#include <vector>

/// Heap data structure for storing timers. This has a tree structure, with
/// every element having 0, 1 or 2 children, and ordered such that every
/// timer's pop time is less than or equal to its children's pop time. This
/// means that:
///
/// * finding the next timer to pop is an O(1) operation
/// * inserting a new timer is an O(log n) operation
/// * deleting a timer is an O(log n) operation
/// * changing the pop time of a timer is an O(log n) operation
class TimerHeap
{
public:
  /// Interface for a timer which can be used in this heap. Subclasses should
  /// implement the get_pop_time method, plus whatever else they need for the
  /// information associated with a timer.
  class Timer
  {
    friend class TimerHeap;

    Timer() :
      _heap(nullptr),
      _heap_position(42) // Arbitrary non-zero number to help detect when a
                         // timer is used without having been inserted into a
                         // heap first.
    {}
    virtual ~Timer() = default;

    /// Time at which this timer pops. This doesn't enforce a particular unit
    /// or epoch - whether it means seconds since 01-01-1970 or milliseconds
    /// since 2000, the heap will just let you access the timer with the
    /// earliest pop time. (Obviously the units and epoch do need to be
    /// consistent between all timers in the same heap.)
    /// 
    /// @return Integer representing the pop time.
    virtual uint64_t get_pop_time() = 0;

  private:
    bool pops_before(Timer* t)
    {
      return get_pop_time() < t->get_pop_time();
    }

    /// Heap which this timer is in, or NULL if it isn't currently in a heap.
    ///
    /// TimerHeap::insert is responsible for updating this field.
    TimerHeap* _heap;

    // The current position of this timer in the heap's underlying array. The
    // TimerHeap is responsible for keeping this up-to-date as it moves the
    // timer around.
    size_t _heap_position;
  };

  /// Adds a timer to the heap. This doesn't take ownership of the timer's
  /// memory - this must be tracked, freed etc. outside of the heap. (The
  /// caller will usually wwant to do this anyway, so that they have a
  /// reference to the timer if they want to update its pop time.)
  ///
  /// Does nothing if this timer is already in the heap.
  /// 
  /// @param t Timer to insert
  void insert(Timer* t);

  /// Removes a timer from the heap. This does not free the timer's memory.
  ///
  /// @param t Timer to remove.
  ///
  /// @returns True if the timer was  removed, False if the timer was not in
  /// the heap.
  bool remove(Timer* t);

  /// Moves the timer up or down as necessary to ensure that this timer is
  /// larger than its parent and smaller than its children (i.e. to ensure the
  /// heap property). Should be called after any operation on a timer that
  /// might have violated the heap property, such as:
  ///
  /// * swapping a timer from the bottom of the tree into an arbitrary position, as part of a delete operation
  /// * changing a timer's pop time
  ///
  /// @param t The timer to move to the right place in the heap.
  void rebalance(Timer* t);

  /// Removes all timers from the heap.
  void clear() { _heap_store.clear(); }
  
  /// @return True if there are no timers in the heap, False otherwise.
  bool empty() { return _heap_store.empty(); }

  /// Returns the timer which will pop next, or NULL if the heap is empty.
  ///
  /// This does not delete the timer from the heap (because callers might not
  /// want to execute the timer immediately - they may want to check the pop
  /// time, and then do some other operation if no timers are going to pop for
  /// a while). If this timer gets used, the caller should call remove() on it.
  Timer* get_next_timer();

private:
  // See https://en.wikipedia.org/wiki/Binary_heap#Heap_implementation for heap
  // index arithmetic.
  inline size_t left_child_of(size_t index) { return (index * 2) + 1; }
  inline size_t right_child_of(size_t index) { return (index * 2) + 2; }
  inline size_t parent_of(size_t index) { return (index - 1) / 2; }

  /// Restores the heap property by swapping the element at index with its
  /// parent (i.e. moving it upwards) until it is smaller than its parent.
  /// 
  /// @param index The index of the timer to move.
  void heapify_upwards(size_t index);

  /// Restores the heap property by swapping the element at index with its
  /// largest child (i.e. moving it downwards) until it is larger than both its
  /// children.
  /// 
  /// @param index The index of the timer to move.
  void heapify_downwards(size_t index);

  /// Swaps the element at `index` with its parent.
  ///
  /// Callers should verify that:
  /// * this element has a parent (i.e. index != 0)
  /// * doing this swap is necessary to restore the heap property (i.e. the element at index currently pops before its parent)
  /// 
  /// @param index The index of the timer to move.
  void swap_with_parent(size_t index);

  /// Underlying storage for the heap.
  std::vector<Timer*> _heap_store;
};

// Basic implementation of a timer which allows setting and updating the pop
// time.
class SimpleTimer : public TimerHeap::Timer
{
  SimpleTimer(uint64_t pop_time) :
    _pop_time(pop_time)
  {}

  void update_pop_time(uint64_t new_pop_time)
  {
    _pop_time = new_pop_time;

    // This timer probably isn't in the right place in the heap any more, so
    // fix that.
    _heap->rebalance(this);
  }

  uint64_t get_pop_time() { return _pop_time; };

private:
  uint64_t _pop_time;
};

#endif
