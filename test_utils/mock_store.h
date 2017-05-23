/**
 * @file mock_store.h Mock store class
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_STORE_H_
#define MOCK_STORE_H_

#include "gmock/gmock.h"

#include "store.h"

class MockStore : public Store
{
public:
  MockStore() {};
  virtual ~MockStore() {};

  MOCK_METHOD5(get_data, Status(const std::string& table,
                                const std::string& key,
                                std::string& data,
                                uint64_t& cas,
                                SAS::TrailId trail));
  MOCK_METHOD6(set_data, Status(const std::string& table,
                                const std::string& key,
                                const std::string& data,
                                uint64_t cas,
                                int expiry,
                                SAS::TrailId trail));
  MOCK_METHOD3(delete_data, Status(const std::string& table,
                                   const std::string& key,
                                   SAS::TrailId trail));
};

#endif

