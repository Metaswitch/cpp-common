/**
 * @file sip_common.hpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <map>
#include <string>
#include <sstream>
#include "gtest/gtest.h"

extern "C" {
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
}

class SipCommonTest : public ::testing::Test
{
public:
  SipCommonTest();
  virtual ~SipCommonTest();

  static void SetUpTestCase();
  static void TearDownTestCase();

protected:

  /// Class containing transport factories for the various ports.
  class TransportFactory
  {
  public:
    TransportFactory();
    ~TransportFactory();
  };

  /// Abstraction of a transport flow used for injecting or receiving SIP
  /// messages.
  class TransportFlow
  {
  public:
    enum Protocol {TCP, UDP};

    TransportFlow(Protocol protocol,
                  int local_port,
                  const char* addr,
                  int port);
    ~TransportFlow();

    static void reset();

    static pjsip_tpfactory* tcp_factory(int port);

  private:
    static std::map<int, pjsip_tpfactory*> _tcp_factories;

    pjsip_transport* _transport;
    pj_sockaddr _rem_addr;
  };

  static SipCommonTest* _current_instance;

  /// Build an incoming SIP packet.
  pjsip_rx_data* build_rxdata(const std::string& msg,
                              TransportFlow* tp = _tp_default,
                              pj_pool_t* rdata_pool = NULL);

  /// Parse an incoming SIP message.
  void parse_rxdata(pjsip_rx_data* rdata);

  /// Parse a string containing a SIP message into a pjsip_msg.
  pjsip_msg* parse_msg(const std::string& msg);

private:
  /// The transport we usually use when injecting messages.
  static TransportFlow* _tp_default;
  static pj_caching_pool _cp;
  static pj_pool_t* _pool;
  static pjsip_endpoint* _endpt;
};
