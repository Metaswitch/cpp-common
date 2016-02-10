/**
 * @file mock_chronos_connection.h
 *
 * Project Clearwater - IMS in the cloud.
 * Copyright (C) 2015 Metaswitch Networks Ltd
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
