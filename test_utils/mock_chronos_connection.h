/**
 * @file mock_chronos_connection.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_CHRONOS_CONNECTION_H__
#define MOCK_CHRONOS_CONNECTION_H__

#include "gmock/gmock.h"
#include "chronosconnection.h"

using ::testing::_;
using ::testing::SetArgReferee;
using ::testing::DoAll;
using ::testing::AnyNumber;
using ::testing::Return;

class MockChronosConnection : public ChronosConnection
{
public:
  MockChronosConnection();
  MockChronosConnection(const std::string& chronos);

  // This sets the Mock to accept any post/put/delete and return 200 OK
  void accept_all_requests()
  {
    ON_CALL(*this, send_post(_, _, _, _, _, _, _)).
      WillByDefault(DoAll(SetArgReferee<0>("TIMER_ID"),
            Return(HTTP_OK)));
    ON_CALL(*this, send_post(_, _, _, _, _, _)).
      WillByDefault(DoAll(SetArgReferee<0>("TIMER_ID"),
            Return(HTTP_OK)));
    ON_CALL(*this, send_put(_, _, _, _, _, _, _)).
      WillByDefault(DoAll(SetArgReferee<0>("TIMER_ID"),
            Return(HTTP_OK)));
    ON_CALL(*this, send_put(_, _, _, _, _, _)).
      WillByDefault(DoAll(SetArgReferee<0>("TIMER_ID"),
            Return(HTTP_OK)));
    ON_CALL(*this, send_delete("TIMER_ID", _)).
      WillByDefault(Return(HTTP_OK));
    EXPECT_CALL(*this, send_post(_,_,_,_,_,_,_)).Times(AnyNumber());
    EXPECT_CALL(*this, send_post(_,_,_,_,_,_)).Times(AnyNumber());
    EXPECT_CALL(*this, send_put(_,_,_,_,_,_,_)).Times(AnyNumber());
    EXPECT_CALL(*this, send_put(_,_,_,_,_,_)).Times(AnyNumber());
    EXPECT_CALL(*this, send_delete(_,_)).Times(AnyNumber());
  };

  MOCK_METHOD2(send_delete, HTTPCode(const std::string&, SAS::TrailId));
  MOCK_METHOD7(send_put, HTTPCode(std::string&, uint32_t, uint32_t, const std::string&, const std::string&, SAS::TrailId, const std::map<std::string, uint32_t>&));
  MOCK_METHOD7(send_post, HTTPCode(std::string&, uint32_t, uint32_t, const std::string&, const std::string&, SAS::TrailId, const std::map<std::string, uint32_t>&));
  MOCK_METHOD6(send_put, HTTPCode(std::string&, uint32_t, const std::string&, const std::string&, SAS::TrailId, const std::map<std::string, uint32_t>&));
  MOCK_METHOD6(send_post, HTTPCode(std::string&, uint32_t, const std::string&, const std::string&, SAS::TrailId, const std::map<std::string, uint32_t>&));
};

#endif
