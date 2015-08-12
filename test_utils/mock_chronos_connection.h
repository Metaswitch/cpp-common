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
MockChronosConnection(const std::string& chronos) : ChronosConnection(chronos, "localhost:10888", NULL, NULL)
  {
    ON_CALL(*this, send_post(_, _, _, _, _, _)).
      WillByDefault(DoAll(SetArgReferee<0>("TIMER_ID"),
            Return(HTTP_OK)));
    ON_CALL(*this, send_post(_, _, _, _, _)).
      WillByDefault(DoAll(SetArgReferee<0>("TIMER_ID"),
            Return(HTTP_OK)));
    ON_CALL(*this, send_put(_, _, _, _, _, _)).
      WillByDefault(DoAll(SetArgReferee<0>("TIMER_ID"),
            Return(HTTP_OK)));
    ON_CALL(*this, send_put(_, _, _, _, _)).
      WillByDefault(DoAll(SetArgReferee<0>("TIMER_ID"),
            Return(HTTP_OK)));
    ON_CALL(*this, send_delete("TIMER_ID", _)).
      WillByDefault(Return(HTTP_OK));
    EXPECT_CALL(*this, send_post(_,_,_,_,_,_)).Times(AnyNumber());
    EXPECT_CALL(*this, send_post(_,_,_,_,_)).Times(AnyNumber());
    EXPECT_CALL(*this, send_put(_,_,_,_,_,_)).Times(AnyNumber());
    EXPECT_CALL(*this, send_put(_,_,_,_,_)).Times(AnyNumber());
    EXPECT_CALL(*this, send_delete(_,_)).Times(AnyNumber());
  };
  MOCK_METHOD2(send_delete, HTTPCode(const std::string&, SAS::TrailId));
  MOCK_METHOD6(send_put, HTTPCode(std::string&, uint32_t, uint32_t, const std::string&, const std::string&, SAS::TrailId));
  MOCK_METHOD6(send_post, HTTPCode(std::string&, uint32_t, uint32_t, const std::string&, const std::string&, SAS::TrailId));
  MOCK_METHOD5(send_put, HTTPCode(std::string&, uint32_t, const std::string&, const std::string&, SAS::TrailId));
  MOCK_METHOD5(send_post, HTTPCode(std::string&, uint32_t, const std::string&, const std::string&, SAS::TrailId));
};

#endif
