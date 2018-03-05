/**
 * @file faketransport_tcp.hpp
 *
 * Copyright (C) Metaswitch Networks 2014
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */
#ifndef __PJSIP_TRANSPORT_FAKE_TCP_H__
#define __PJSIP_TRANSPORT_FAKE_TCP_H__

/**
 * @file sip_transport_fake.h
 * @brief SIP FAKE Transport.
 */

extern "C" {
#include <pjsip/sip_transport.h>
#include <pj/sock_qos.h>
}


PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_TRANSPORT_FAKE_TCP FAKE_TCP Transport
 * @ingroup PJSIP_TRANSPORT
 * @brief API to create and register FAKE_TCP transport.
 * @{
 * The functions below are used to create FAKE_TCP transport and register
 * the transport to the framework.
 */

/**
 * A newer variant of #pjsip_fake_tcp_transport_start(), which allows specifying
 * the published/public address of the TCP transport.
 *
 * @param endpt		The SIP endpoint.
 * @param local		Optional local address to bind, or specify the
 *			address to bind the server socket to. Both IP
 *			interface address and port fields are optional.
 *			If IP interface address is not specified, socket
 *			will be bound to PJ_INADDR_ANY. If port is not
 *			specified, socket will be bound to any port
 *			selected by the operating system.
 * @param a_name	Optional published address, which is the address to be
 *			advertised as the address of this SIP transport.
 *			If this argument is NULL, then the bound address
 *			will be used as the published address.
 * @param async_cnt	Number of simultaneous asynchronous accept()
 *			operations to be supported. It is recommended that
 *			the number here corresponds to the number of
 *			processors in the system (or the number of SIP
 *			worker threads).
 * @param p_factory	Optional pointer to receive the instance of the
 *			SIP TCP transport factory just created.
 *
 * @return		PJ_SUCCESS when the transport has been successfully
 *			started and registered to transport manager, or
 *			the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_fake_tcp_transport_start2(pjsip_endpoint *endpt,
                                                     const pj_sockaddr_in *local,
                                                     const pjsip_host_port *a_name,
                                                     unsigned async_cnt,
                                                     pjsip_tpfactory **p_factory);

/**
 * Settings to be specified when creating the FAKE_TCP transport. Application
 * should initialize this structure with its default values by calling
 * pjsip_fake_tcp_transport_cfg_default().
 */
typedef struct pjsip_fake_tcp_transport_cfg
{
    /**
     * Address family to use. Valid values are pj_AF_INET() and
     * pj_AF_INET6(). Default is pj_AF_INET().
     */
    int			af;

    /**
     * Optional address to bind the socket to. Default is to bind to
     * PJ_INADDR_ANY and to any available port.
     */
    pj_sockaddr		bind_addr;

    /**
     * Optional published address, which is the address to be
     * advertised as the address of this SIP transport.
     * By default the bound address will be used as the published address.
     */
    pjsip_host_port	addr_name;

    /**
     * Number of simultaneous asynchronous accept() operations to be
     * supported. It is recommended that the number here corresponds to
     * the number of processors in the system (or the number of SIP
     * worker threads).
     *
     * Default: 1
     */
    unsigned	       async_cnt;

    /**
     * QoS traffic type to be set on this transport. When application wants
     * to apply QoS tagging to the transport, it's preferable to set this
     * field rather than \a qos_param fields since this is more portable.
     *
     * Default is QoS not set.
     */
    pj_qos_type		qos_type;

    /**
     * Set the low level QoS parameters to the transport. This is a lower
     * level operation than setting the \a qos_type field and may not be
     * supported on all platforms.
     *
     * Default is QoS not set.
     */
    pj_qos_params	qos_params;

} pjsip_fake_tcp_transport_cfg;


/**
 * Initialize pjsip_fake_tcp_transport_cfg structure with default values for
 * the specifed address family.
 *
 * @param cfg		The structure to initialize.
 * @param af		Address family to be used.
 */
PJ_DECL(void) pjsip_fake_tcp_transport_cfg_default(pjsip_fake_tcp_transport_cfg *cfg,
					      int af);

/**
 * Another variant of #pjsip_fake_tcp_transport_start().
 *
 * @param endpt		The SIP endpoint.
 * @param cfg		FAKE_TCP transport settings. Application should initialize
 *			this setting with #pjsip_fake_tcp_transport_cfg_default().
 * @param p_factory	Optional pointer to receive the instance of the
 *			SIP FAKE_TCP transport factory just created.
 *
 * @return		PJ_SUCCESS when the transport has been successfully
 *			started and registered to transport manager, or
 *			the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_fake_tcp_transport_start3(
					pjsip_endpoint *endpt,
					const pjsip_fake_tcp_transport_cfg *cfg,
					pjsip_tpfactory **p_factory
					);


/**
 * Simulates an incoming TCP connection.
 *
 * @param factory       The factory used to create the connection.
 * @param src_addr      The source IP address of the connection.
 * @param src_addr_len  The length of the source IP address. (Not used).
 * @param p_transport   Receives the transport instance created.
 *
 */
PJ_DECL(pj_status_t) pjsip_fake_tcp_accept(pjsip_tpfactory* factory,
                                           const pj_sockaddr_t* src_addr,
                                           int src_addr_len,
                                           pjsip_transport** p_transport);


/**
 * Shut down connection (driven by connection error or EOF).
 *
 */
void fake_tcp_init_shutdown(struct fake_tcp_transport *fake_tcp, pj_status_t status);

PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJSIP_TRANSPORT_FAKE_TCP_H__ */
