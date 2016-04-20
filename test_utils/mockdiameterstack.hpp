/**
 * @file mockdiameterstack.h Mock HTTP stack.
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

#ifndef MOCKDIAMETERSTACK_H__
#define MOCKDIAMETERSTACK_H__

#include "gmock/gmock.h"
#include "diameterstack.h"

class MockDiameterStack : public Diameter::Stack
{
public:
  MOCK_METHOD0(initialize, void());
  MOCK_METHOD3(register_handler, void(const Diameter::Dictionary::Application&, const Diameter::Dictionary::Message&, HandlerInterface*));
  MOCK_METHOD1(register_fallback_handler, void(const Diameter::Dictionary::Application&));
  MOCK_METHOD2(register_peer_hook_hdlr, void(std::string, Diameter::PeerConnectionCB));
  MOCK_METHOD1(unregister_peer_hook_hdlr, void(std::string));
  MOCK_METHOD2(register_rt_out_cb, void(std::string, Diameter::RtOutCB));
  MOCK_METHOD1(unregister_rt_out_cb, void(std::string));
  MOCK_METHOD0(start, void());
  MOCK_METHOD0(stop, void());
  MOCK_METHOD0(wait_stopped, void());
  MOCK_METHOD2(send, void(struct msg*, SAS::TrailId));
  MOCK_METHOD2(send, void(struct msg*, Diameter::Transaction*));
  MOCK_METHOD3(send, void(struct msg*, Diameter::Transaction*, unsigned int timeout_ms));
  MOCK_METHOD1(add, bool(Diameter::Peer*));
  MOCK_METHOD1(remove, void(Diameter::Peer*));
  MOCK_METHOD0(set_allow_connections, void());
  MOCK_METHOD0(close_connections, void());
  MOCK_METHOD2(peer_count, void(int, int));
};

#endif
