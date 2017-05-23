/**
 * @file mockdiameterstack.h Mock HTTP stack.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
