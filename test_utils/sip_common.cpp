/**
 * @file sip_common.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */


#include <string>
#include "gtest/gtest.h"
#include "arpa/inet.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <boost/lexical_cast.hpp>

extern "C" {
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
}

#include "faketransport_udp.hpp"
#include "faketransport_tcp.hpp"
#include "test_interposer.hpp"
#include "sip_common.hpp"

using namespace std;

/// Runs before each test.
SipCommonTest::SipCommonTest()
{
  EXPECT_TRUE(_current_instance == NULL) << "Can't run two SipCommonTests in parallel";
  _current_instance = this;
}

/// Runs after each test.
SipCommonTest::~SipCommonTest()
{
  _current_instance = NULL;
}

SipCommonTest::TransportFlow* SipCommonTest::_tp_default;
SipCommonTest* SipCommonTest::_current_instance;
pj_caching_pool SipCommonTest::_cp;
pj_pool_t* SipCommonTest::_pool;
pjsip_endpoint* SipCommonTest::_endpt;

/// Automatically run once, before the first test.
void SipCommonTest::SetUpTestCase()
{
  // Must init PJLIB first:
  pj_init();

  // Then init PJLIB-UTIL:
  pjlib_util_init();

  // Must create a pool factory before we can allocate any memory.
  pj_caching_pool_init(&_cp, &pj_pool_factory_default_policy, 0);

  // Create the endpoint.
  pjsip_endpt_create(&_cp.factory, NULL, &_endpt);

  // Create pool for the application
  _pool = pj_pool_create(&_cp.factory,
                         "sip_common",
                         4000,
                         4000,
                         NULL);

  // Get a default TCP transport flow to use for injection.  Give it a dummy address.
  _tp_default = new TransportFlow(TransportFlow::Protocol::TCP,
                                  5058,
                                  "0.0.0.0",
                                  5060);
}

/// Automatically run once, after the last test.
void SipCommonTest::TearDownTestCase()
{
  // Delete the default TCP transport flow.
  delete _tp_default;

  pjsip_endpt_destroy(_endpt);
  pj_pool_release(_pool);
  pj_caching_pool_destroy(&_cp);
  pj_shutdown();

  // Clear out any UDP transports and TCP factories that have been created.
  TransportFlow::reset();
}

std::map<int, pjsip_tpfactory*> SipCommonTest::TransportFlow::_tcp_factories;

void SipCommonTest::TransportFlow::reset()
{
  _tcp_factories.clear();
}

pjsip_tpfactory* SipCommonTest::TransportFlow::tcp_factory(int port)
{
  if (_tcp_factories[port] == NULL)
  {
    pj_sockaddr_in addr;
    pjsip_host_port published_name;
    pjsip_tpfactory* tcp_factory;

    memset(&addr, 0, sizeof(pj_sockaddr_in));
    addr.sin_family = pj_AF_INET();
    addr.sin_addr.s_addr = 0;
    addr.sin_port = pj_htons((pj_uint16_t)port);

    published_name.host = pj_str("127.0.0.1");
    published_name.port = port;

    pjsip_fake_tcp_transport_start2(_endpt,
                                    &addr,
                                    &published_name,
                                    50,
                                    &tcp_factory);

    _tcp_factories[port] = tcp_factory;
  }

  return _tcp_factories[port];
}


SipCommonTest::TransportFlow::TransportFlow(Protocol protocol, int local_port, const char* addr, int port)
{
  pj_str_t addr_str = pj_str(const_cast<char*>(addr));
  pj_sockaddr_init(PJ_AF_INET, &_rem_addr, &addr_str, port);

  pj_status_t status;
  pjsip_tpfactory *factory = tcp_factory(local_port);
  status = pjsip_fake_tcp_accept(factory,
                                 (pj_sockaddr_t*)&_rem_addr,
                                 sizeof(pj_sockaddr_in),
                                 &_transport);
  pjsip_transport_add_ref(_transport);
  EXPECT_EQ(PJ_SUCCESS, status);
}


SipCommonTest::TransportFlow::~TransportFlow()
{
  if (!strcmp(_transport->type_name, "TCP"))
  {
    pjsip_transport_dec_ref(_transport);
    fake_tcp_init_shutdown((fake_tcp_transport*)_transport, PJ_EEOF);
  }
}

pjsip_rx_data* SipCommonTest::build_rxdata(const string& msg, TransportFlow* tp, pj_pool_t* rdata_pool)
{
  pjsip_rx_data* rdata = PJ_POOL_ZALLOC_T(_pool, pjsip_rx_data);

  if (rdata_pool == NULL)
  {
    rdata_pool = _pool;
  }

  // Init transport info part.
  rdata->tp_info.pool = rdata_pool;
  rdata->tp_info.transport = tp->_transport;
  rdata->tp_info.tp_data = NULL;
  rdata->tp_info.op_key.rdata = rdata;
  pj_ioqueue_op_key_init(&rdata->tp_info.op_key.op_key,
                         sizeof(pj_ioqueue_op_key_t));

  // Copy in message bytes.
  rdata->pkt_info.packet = (char*)pj_pool_alloc(rdata->tp_info.pool, strlen(msg.data()) + 1);
  strcpy(rdata->pkt_info.packet, msg.data());
  rdata->pkt_info.len = msg.length();

  // Fill in packet info part.
  rdata->pkt_info.src_addr = tp->_rem_addr;
  rdata->pkt_info.src_addr_len = sizeof(rdata->pkt_info.src_addr);
  pj_sockaddr* rem_addr = &tp->_rem_addr;
  pj_sockaddr_print(rem_addr, rdata->pkt_info.src_name,
                    sizeof(rdata->pkt_info.src_name), 0);
  rdata->pkt_info.src_port = pj_sockaddr_get_port(rem_addr);

  pj_gettimeofday(&rdata->pkt_info.timestamp);

  return rdata;
}

void SipCommonTest::parse_rxdata(pjsip_rx_data* rdata)
{
  // Parse message.
  pj_bzero(&rdata->msg_info, sizeof(rdata->msg_info));
  pj_list_init(&rdata->msg_info.parse_err);
  rdata->msg_info.msg = pjsip_parse_rdata(rdata->pkt_info.packet, rdata->pkt_info.len, rdata);
  if (!pj_list_empty(&rdata->msg_info.parse_err))
  {
    // Parse error!  See sip_transport.c
    /* Gather syntax error information */
    pjsip_parser_err_report* err = rdata->msg_info.parse_err.next;
    while (err != &rdata->msg_info.parse_err)
    {
      printf("%s exception when parsing '%.*s' "
             "header on line %d col %d\n",
             pj_exception_id_name(err->except_code),
             (int)err->hname.slen, err->hname.ptr,
             err->line, err->col);
      err = err->next;
    }
    // Fail out here
    ASSERT_FALSE(true);
  }

  ASSERT_FALSE(rdata->msg_info.msg == NULL);

  // Perform basic header checking.
  EXPECT_FALSE(rdata->msg_info.cid == NULL);
  EXPECT_FALSE(rdata->msg_info.cid->id.slen == 0);
  EXPECT_FALSE(rdata->msg_info.from == NULL);
  EXPECT_FALSE(rdata->msg_info.to == NULL);
  EXPECT_FALSE(rdata->msg_info.via == NULL);
  EXPECT_FALSE(rdata->msg_info.cseq == NULL);

  // Fill in VIA.
  if (rdata->msg_info.msg->type == PJSIP_REQUEST_MSG)
  {
    pj_strdup2(rdata->tp_info.pool,
               &rdata->msg_info.via->recvd_param,
               rdata->pkt_info.src_name);

    if (rdata->msg_info.via->rport_param == 0)
    {
      rdata->msg_info.via->rport_param = rdata->pkt_info.src_port;
    }
  }
  else
  {
    EXPECT_FALSE(rdata->msg_info.msg->line.status.code < 100);
    EXPECT_FALSE(rdata->msg_info.msg->line.status.code >= 700);
  }
}

pjsip_msg* SipCommonTest::parse_msg(const std::string& msg)
{
  pjsip_rx_data* rdata = build_rxdata(msg);
  parse_rxdata(rdata);
  return rdata->msg_info.msg;
}
