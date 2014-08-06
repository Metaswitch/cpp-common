/**
 * @file sip_common.hpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014  Metaswitch Networks Ltd
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
