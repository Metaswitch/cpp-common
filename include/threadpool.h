/**
 * @file threadpool.h implementation of a simple thread pool.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <functional>

#include <eventq.h>
#include "exception_handler.h"
#include <log.h>

#ifndef THREADPOOL_H__
#define THREADPOOL_H__

// A thread pool that receives work items on one thread, and passes them to be
// processed on one of a pool of threads.
//
// - Create a subclass of ThreadPool.
// - Create a new instance of this class (called `pool` for example).
// - Call pool->start() to start the pool and create its threads.
// - Call pool->add_work to give the pool work to process.
// - Call pool->stop() to stop the pool and terminate its threads.
// - (optionally) Call pool->join() to wait until the pool is fully stopped.
//
// Note that start may only be called once. Once the pool has been stopped it
// cannot be restarted.
//
// The start(), stop(), and join() methods are not threadsafe and should not be
// called simultaneously.
template <class T>
class ThreadPool
{
public:
  // Create the thread pool.
  //
  // @param num_threads the number of threads in the pool.
  // @param max_queue the number of work items that can be queued waiting for a
  //   free thread (0 => no limit).
  ThreadPool(unsigned int num_threads,
             ExceptionHandler* exception_handler,
             void (*callback)(T),
             unsigned int max_queue = 0) :
    _num_threads(num_threads),
    _exception_handler(exception_handler),
    _threads(0),
    _queue(max_queue),
    _callback(callback)
  {}

  // Destroy the thread pool.
  virtual ~ThreadPool() {};

  // Start the thread pool by creating the required number of worker threads.
  //
  // @return whether the thread pool started successfully.
  bool start()
  {
    bool success = true;
    pthread_t thread_handle;

    for (unsigned int ii = 0; ii < _num_threads; ++ii)
    {
      int rc = pthread_create(&thread_handle,
                              NULL,
                              static_worker_thread_func,
                              this);
      if (rc == 0)
      {
        _threads.push_back(thread_handle);
      }
      else
      {
        TRC_ERROR("Failed to create thread in thread pool");

        // Terminate the pool so that all existing threads will exit.
        _queue.terminate();
        _threads.clear();

        success = false;
        break;
      }
    }

    return success;
  }

  // Stop the thread pool and shutdown the worker threads.  Work items on the
  // queue are not guaranteed to be processed.
  void stop()
  {
    // Purge any pending work items (to ensure the threads stop promptly) and
    // then terminate the queue. This will cause any idle worker threads (those
    // currently blocked getting work from the queue) to wake up and exit.
    _queue.purge();
    _queue.terminate();
  }

  // Wait for the threadpool to shutdown.
  void join()
  {
    for (unsigned int ii = 0; ii < _threads.size(); ++ii)
    {
      pthread_join(_threads[ii], NULL);
    }
  }

  // Add a work item to the thread pool.
  //
  // @param work the work item to add.
  void add_work(T& work)
  {
    _queue.push(work);
  }

  // Add a work item to the thread pool by moving it into the pool.
  //
  // @param work the work item to add.
  void add_work(T&& work)
  {
    _queue.push(work);
  }

private:
  unsigned int _num_threads;
  ExceptionHandler* _exception_handler;
  std::vector<pthread_t> _threads;
  eventq<T> _queue;

  // Recovery function provided by the callers
  void (*_callback)(T);

  // Static worker thread function that is passed into pthread_create.
  //
  // We can't use a mem_fun here as we can't convert the resulting mem_fun_t to
  // the void *(*)(void *) required by pthreads.
  //
  // @param pool pointer to the worker pool.
  // @return NULL (required by the pthreads API).
  static void *static_worker_thread_func(void *pool)
  {
    ((ThreadPool<T> *)pool)->worker_thread_func();
    return NULL;
  }

  // Take on work item off the queue an process it. This is called repeatedly by
  // the worker threads until it returns false (meaning the work queue has been
  // closed).
  //
  // This can also be used in UTs to control execution of the thread pool.
  bool run_once()
  {
    T work;
    bool got_work = _queue.pop(work);

    if (got_work)
    {
      CW_TRY
      {
        process_work(work);
      }
      CW_EXCEPT(_exception_handler)
      {
        _callback(work);
      }
      CW_END
    }

    return got_work;
  }

  // Function executed by a single worker thread. This loops pulling work off
  // the queue and processing it.
  void worker_thread_func()
  {
    bool got_work;

    // Startup hook.
    on_thread_startup();

    do
    {
      got_work = run_once();

      // If we haven't got any work then the queue must have been terminated,
      // which in turn means the threadpool has been shut down.  Exit the loop.
    } while (got_work);

    // Shutdown hook.
    on_thread_shutdown();
  }

  // (Optional) thread startup hook.  This is called by each worker thread just
  // after it starts up.
  //
  // The default implementation of this hook is a no-op.
  virtual void on_thread_startup() {};

  // (Optional) thread shutdown hook.  This is called by each worker thread just
  // before it exits.
  //
  // The default implementation of this hook is a no-op.
  virtual void on_thread_shutdown() {};

  // Process a work item. This method must be overridden by the subclass.
  virtual void process_work(T& work) = 0;
};


/// An alternative thread pool where the work items are callable objects. When a
/// thread processes a work item it just calls the object. This allows thread
/// pools to be used ergonomically with lambdas and std::binds.
class FunctorThreadPool : public ThreadPool<std::function<void()>>
{
public:
  /// Just use the `ThreadPool` constructor.
  using ThreadPool<std::function<void()>>::ThreadPool;

  virtual ~FunctorThreadPool() {};

  void process_work(std::function<void()>& callable)
  {
    callable();
  }
};

#endif
