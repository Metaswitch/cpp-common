/**
 * @file eventq.h Template definition for event queue
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef EVENTQ__
#define EVENTQ__

#include <pthread.h>
#include <errno.h>
#include <time.h>

#include <queue>

#include "log.h"

template<class T>
class eventq
{
public:
  /// Create an event queue.
  ///
  /// @param max_queue maximum size of event queue, zero is unlimited.
  eventq(unsigned int max_queue=0, bool open=true) :
    _open(open),
    _max_queue(max_queue),
    _q(),
    _writers(0),
    _readers(0),
    _terminated(false),
    _deadlock_threshold(0)
  {
    pthread_mutex_init(&_m, NULL);
    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
    pthread_cond_init(&_w_cond, &cond_attr);
    pthread_cond_init(&_r_cond, &cond_attr);
    pthread_condattr_destroy(&cond_attr);
  }

  ~eventq()
  {
  }

  /// Open the queue for new inputs.
  void open()
  {
    _open = true;
  }

  /// Close the queue to new inputs.
  void close()
  {
    _open = false;
  }

  /// Send a termination signal via the queue.
  void terminate()
  {
    pthread_mutex_lock(&_m);

    _terminated = true;

    // Are there any readers waiting?
    if (_readers > 0)
    {
      // Signal all waiting readers.  Can do this before releasing the mutex
      // as we're relying on wait-morphing being supported by the OS (so
      // there will be no spurious context switches).
      pthread_cond_broadcast(&_r_cond);
    }

    pthread_mutex_unlock(&_m);
  }

  /// Indicates whether the queue has been terminated.
  bool is_terminated()
  {
    pthread_mutex_lock(&_m);
    bool terminated = _terminated;
    pthread_mutex_unlock(&_m);
    return terminated;
  }

  /// Enables deadlock detection on the queue with the specified threshold
  /// (in milliseconds).
  void set_deadlock_threshold(unsigned long threshold_ms)
  {
    pthread_mutex_lock(&_m);

    // Store the threshold.
    _deadlock_threshold = threshold_ms;

    // Set the service time to the current time as we don't update it while
    // detection is disabled.
    clock_gettime(CLOCK_MONOTONIC, &_service_time);

    pthread_mutex_unlock(&_m);
  }

  /// Returns the deadlocked state of the queue.
  bool is_deadlocked()
  {
    bool deadlocked = false;

    pthread_mutex_lock(&_m);

    if ((_deadlock_threshold > 0) &&
        (!_q.empty()))
    {
      // Deadlock detection is enabled, and the queue is not empty, so check
      // how long it has been since the queue was last serviced.
      struct timespec now;
      if (clock_gettime(CLOCK_MONOTONIC, &now) == 0)
      {
        uint64_t service_time = (_service_time.tv_sec * 1000) +
                                (_service_time.tv_nsec / 1000000);
        uint64_t now_time = (now.tv_sec * 1000) +
                            (now.tv_nsec / 1000000);

        // Check that the current time is greater than the last serviced time -
        // if it's not then we can't be deadlocked.
        if ((now_time > service_time) &&
            ((now_time - service_time) > _deadlock_threshold))
        {
          TRC_ERROR("Queue is deadlocked - service delay %ld > threshold %ld",
                    service_time - now_time, _deadlock_threshold);
          TRC_DEBUG("  Last service time = %d.%ld", _service_time.tv_sec, _service_time.tv_nsec);
          TRC_DEBUG("  Now = %d.%ld", now.tv_sec, now.tv_nsec);
          deadlocked = true;
        }
      }
    }

    pthread_mutex_unlock(&_m);

    return deadlocked;
  }

  /// Purges all the events currently in the queue.
  void purge()
  {
    pthread_mutex_lock(&_m);
    while (!_q.empty())
    {
      _q.pop();
    }
    pthread_mutex_unlock(&_m);
  }

  /// Push an item on to the event queue.
  ///
  /// This may block if the queue is full, and will fail if the queue is closed.
  bool push(T item)
  {
    bool rc = false;

    pthread_mutex_lock(&_m);

    if (_open)
    {
      if (_max_queue != 0)
      {
        while (_q.size() >= _max_queue)
        {
          // Queue is full, so writer must block.
          ++_writers;
          pthread_cond_wait(&_w_cond, &_m);
          --_writers;
        }
      }

      if ((_deadlock_threshold > 0) &&
          (_q.empty()))
      {
        // Deadlock detection is enabled, and we're about to push an item on
        // to an empty queue, so update the service time to the current time.
        // This is done to avoid false positives when the system has been idle
        // for a while.
        clock_gettime(CLOCK_MONOTONIC, &_service_time);
      }

      // Must be space on the queue now.
      _q.push(item);

      // Are there any readers waiting?
      if (_readers > 0)
      {
        pthread_cond_signal(&_r_cond);
      }

      rc = true;
    }

    pthread_mutex_unlock(&_m);

    return rc;
  }

  /// Push an item on to the event queue.
  ///
  /// This will not block, but may discard the event if the queue is full.
  bool push_noblock(T item)
  {
    bool rc = false;

    pthread_mutex_lock(&_m);

    if ((_open) && ((_max_queue == 0) || (_q.size() < _max_queue)))
    {
      if ((_deadlock_threshold > 0) &&
          (_q.empty()))
      {
        // Deadlock detection is enabled, and we're about to push an item on
        // to an empty queue, so update the service time to the current time.
        // This is done to avoid false positives when the system has been idle
        // for a while.
        clock_gettime(CLOCK_MONOTONIC, &_service_time);
      }

      // There is space on the queue.
      _q.push(item);

      // Are there any readers waiting?
      if (_readers > 0)
      {
        pthread_cond_signal(&_r_cond);
      }

      rc = true;
    }

    pthread_mutex_unlock(&_m);

    return rc;
  }

  /// Pop an item from the event queue, waiting indefinitely if it is empty.
  bool pop(T& item)
  {
    pthread_mutex_lock(&_m);

    while ((_q.empty()) && (!_terminated))
    {
      // The queue is empty, so wait for something to arrive.
      ++_readers;
      pthread_cond_wait(&_r_cond, &_m);
      --_readers;
    }

    if (!_q.empty())
    {
      // Something on the queue to receive.
      item = _q.front();
      _q.pop();

      // Are there blocked writers?
      if ((_max_queue != 0) &&
          (_q.size() < _max_queue) &&
          (_writers > 0))
      {
        pthread_cond_signal(&_w_cond);
      }
    }

    if (_deadlock_threshold > 0)
    {
      // Deadlock detection is enabled, so record the time we popped an
      // item off the queue.
      clock_gettime(CLOCK_MONOTONIC, &_service_time);
    }

    pthread_mutex_unlock(&_m);

    return !_terminated;
  }

  /// Pop an item from the event queue, waiting for the specified timeout if
  /// the queue is empty.
  ///
  /// @param timeout Maximum time to wait in milliseconds.
  bool pop(T& item, int timeout)
  {
    pthread_mutex_lock(&_m);

    if ((_q.empty()) && (timeout != 0))
    {
      // The queue is empty and the timeout is non-zero, so wait for
      // something to arrive.
      struct timespec attime;
      if (timeout != -1)
      {
        clock_gettime(CLOCK_MONOTONIC, &attime);
        attime.tv_sec += timeout / 1000;
        attime.tv_nsec += ((timeout % 1000) * 1000000);
        if (attime.tv_nsec >= 1000000000)
        {
          attime.tv_nsec -= 1000000000;
          attime.tv_sec += 1;
        }
      }

      ++_readers;

      while ((_q.empty()) && (!_terminated))
      {
        // The queue is empty, so wait for something to arrive.
        if (timeout != -1)
        {
          int rc = pthread_cond_timedwait(&_r_cond, &_m, &attime);
          if (rc == ETIMEDOUT)
          {
            break;
          }
        }
        else
        {
          pthread_cond_wait(&_r_cond, &_m);
        }
      }

      --_readers;
    }

    if (!_q.empty())
    {
      item = _q.front();
      _q.pop();

      if ((_max_queue != 0) &&
          (_q.size() < _max_queue) &&
          (_writers > 0))
      {
        pthread_cond_signal(&_w_cond);
      }
    }

    if (_deadlock_threshold > 0)
    {
      // Deadlock detection is enabled, so record the time we popped an
      // item off the queue.
      clock_gettime(CLOCK_MONOTONIC, &_service_time);
    }

    pthread_mutex_unlock(&_m);

    return !_terminated;
  }

  /// Peek at the item at the front of the event queue.
  T peek()
  {
    T item;
    pthread_mutex_lock(&_m);
    if (!_q.empty())
    {
      item = _q.front();
    }
    pthread_mutex_unlock(&_m);
    return item;
  }

  int size() const
  {
    return _q.size();
  }

private:

  bool _open;
  unsigned int _max_queue;
  std::queue<T> _q;
  int _writers;
  int _readers;
  bool _terminated;

  // Deadlock detection threshold (in milliseconds).  Zero means deadlock
  // detection is disabled.
  unsigned long _deadlock_threshold;

  // The last time the queue was serviced (that is, an item was removed from
  // the queue).  Note that, to stop false positives after a period where
  // the queue is empty, the service time is reset whenever an item is placed
  // on to an empty queue.  Also, this field is only maintained when deadlock
  // detection is enabled.
  struct timespec _service_time;

  pthread_mutex_t _m;
  pthread_cond_t _w_cond;
  pthread_cond_t _r_cond;

};

#endif
