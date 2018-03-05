/**
 * @file mock_connectionpool.h Mock ConnectionPool<int> for testing
 * ConnectionPool<T>
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "gmock/gmock.h"
#include "connection_pool.h"

class TestableConnectionPool : public ConnectionPool<int>
{
public:
  TestableConnectionPool(time_t max_idle_time_s) : ConnectionPool<int>(max_idle_time_s) {}
  ~TestableConnectionPool()
  {
    destroy_connection_pool();
  }

  void set_free_on_error(bool free_on_error)
  {
    _free_on_error = free_on_error;
  }

protected:
  MOCK_METHOD1(create_connection, int(AddrInfo target));
  MOCK_METHOD2(destroy_connection, void(AddrInfo target, int conn));
};
