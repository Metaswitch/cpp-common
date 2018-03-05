/**
 * @file sip_transport_fake_udp.h
 *
 * Copyright (C) Metaswitch Networks 2014
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef __PJSIP_TRANSPORT_FAKE_UDP_H__
#define __PJSIP_TRANSPORT_FAKE_UDP_H__


extern "C" {
#include <pjsip/sip_transport.h>
}

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_TRANSPORT_FAKE_UDP FAKE_UDP Transport
 * @ingroup PJSIP_TRANSPORT
 * @brief API to create and register FAKE_UDP transport.
 * @{
 * The functions below are used to create FAKE_UDP transport and register
 * the transport to the framework.
 */

/**
 * Flag that can be specified when calling #pjsip_fake_udp_transport_pause() or
 * #pjsip_fake_udp_transport_restart().
 */
enum
{
    /**
     * This flag tells the transport to keep the existing/internal socket
     * handle.
     */
    PJSIP_FAKE_UDP_TRANSPORT_KEEP_SOCKET	= 1,

    /**
     * This flag tells the transport to destroy the existing/internal socket
     * handle. Naturally this flag and PJSIP_FAKE_UDP_TRANSPORT_KEEP_SOCKET are
     * mutually exclusive.
     */
    PJSIP_FAKE_UDP_TRANSPORT_DESTROY_SOCKET	= 2
};

/**
 * Start FAKE_UDP transport.
 *
 * @param endpt		The SIP endpoint.
 * @param local		Optional local address to bind. If this argument
 *			is NULL, the FAKE_UDP transport will be bound to arbitrary
 *			FAKE_UDP port.
 * @param a_name	Published address (only the host and port portion is
 *			used). If this argument is NULL, then the bound address
 *			will be used as the published address.
 * @param async_cnt	Number of simultaneous async operations.
 * @param p_transport	Pointer to receive the transport.
 *
 * @return		PJ_SUCCESS when the transport has been successfully
 *			started and registered to transport manager, or
 *			the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_fake_udp_transport_start(pjsip_endpoint *endpt,
					       const pj_sockaddr_in *local,
					       const pjsip_host_port *a_name,
					       unsigned async_cnt,
					       pjsip_transport **p_transport);


/**
 * Attach IPv4 FAKE_UDP socket as a new transport and start the transport.
 *
 * @param endpt		The SIP endpoint.
 * @param sock		FAKE_UDP socket to use.
 * @param a_name	Published address (only the host and port portion is
 *			used).
 * @param async_cnt	Number of simultaneous async operations.
 * @param p_transport	Pointer to receive the transport.
 *
 * @return		PJ_SUCCESS when the transport has been successfully
 *			started and registered to transport manager, or
 *			the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_fake_udp_transport_attach(pjsip_endpoint *endpt,
						pj_sock_t sock,
						const pjsip_host_port *a_name,
						unsigned async_cnt,
						pjsip_transport **p_transport);


/**
 * Attach IPv4 or IPv6 FAKE_UDP socket as a new transport and start the transport.
 *
 * @param endpt		The SIP endpoint.
 * @param type		Transport type, which is PJSIP_TRANSPORT_FAKE_UDP for IPv4
 *			or PJSIP_TRANSPORT_FAKE_UDP6 for IPv6 socket.
 * @param sock		FAKE_UDP socket to use.
 * @param a_name	Published address (only the host and port portion is
 *			used).
 * @param async_cnt	Number of simultaneous async operations.
 * @param p_transport	Pointer to receive the transport.
 *
 * @return		PJ_SUCCESS when the transport has been successfully
 *			started and registered to transport manager, or
 *			the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_fake_udp_transport_attach2(pjsip_endpoint *endpt,
						 pjsip_transport_type_e type,
						 pj_sock_t sock,
						 const pjsip_host_port *a_name,
						 unsigned async_cnt,
						 pjsip_transport **p_transport);

/**
 * Retrieve the internal socket handle used by the FAKE_UDP transport. Note
 * that this socket normally is registered to ioqueue, so if application
 * wants to make use of this socket, it should temporarily pause the
 * transport.
 *
 * @param transport	The FAKE_UDP transport.
 *
 * @return		The socket handle, or PJ_INVALID_SOCKET if no socket
 *			is currently being used (for example, when transport
 *			is being paused).
 */
PJ_DECL(pj_sock_t) pjsip_fake_udp_transport_get_socket(pjsip_transport *transport);


/**
 * Temporarily pause or shutdown the transport. When transport is being
 * paused, it cannot be used by the SIP stack to send or receive SIP
 * messages.
 *
 * Two types of operations are supported by this function:
 *  - to temporarily make this transport unavailable for SIP uses, but
 *    otherwise keep the socket handle intact. Application then can
 *    retrieve the socket handle with #pjsip_fake_udp_transport_get_socket()
 *    and use it to send/receive application data (for example, STUN
 *    messages). In this case, application should specify
 *    PJSIP_FAKE_UDP_TRANSPORT_KEEP_SOCKET when calling this function, and
 *    also to specify this flag when calling #pjsip_fake_udp_transport_restart()
 *    later.
 *  - to temporarily shutdown the transport, including closing down
 *    the internal socket handle. This is useful for example to
 *    temporarily suspend the application for an indefinite period. In
 *    this case, application should specify PJSIP_FAKE_UDP_TRANSPORT_DESTROY_SOCKET
 *    flag when calling this function, and specify a new socket when
 *    calling #pjsip_fake_udp_transport_restart().
 *
 * @param transport	The FAKE_UDP transport.
 * @param option	Pause option.
 *
 * @return		PJ_SUCCESS if transport is paused successfully,
 *			or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_fake_udp_transport_pause(pjsip_transport *transport,
					       unsigned option);

/**
 * Restart the transport. Several operations are supported by this function:
 *  - if transport was made temporarily unavailable to SIP stack with
 *    pjsip_fake_udp_transport_pause() and PJSIP_FAKE_UDP_TRANSPORT_KEEP_SOCKET,
 *    application can make the transport available to the SIP stack
 *    again, by specifying PJSIP_FAKE_UDP_TRANSPORT_KEEP_SOCKET flag here.
 *  - if application wants to replace the internal socket with a new
 *    socket, it must specify PJSIP_FAKE_UDP_TRANSPORT_DESTROY_SOCKET when
 *    calling this function, so that the internal socket will be destroyed
 *    if it hasn't been closed. In this case, application has two choices
 *    on how to create the new socket: 1) to let the transport create
 *    the new socket, in this case the \a sock option should be set
 *    to \a PJ_INVALID_SOCKET and optionally the \a local parameter can be
 *    filled with the desired address and port where the new socket
 *    should be bound to, or 2) to specify its own socket to be used
 *    by this transport, by specifying a valid socket in \a sock argument
 *    and set the \a local argument to NULL. In both cases, application
 *    may specify the published address of the socket in \a a_name
 *    argument.
 *
 * @param transport	The FAKE_UDP transport.
 * @param option	Restart option.
 * @param sock		Optional socket to be used by the transport.
 * @param local		The address where the socket should be bound to.
 *			If this argument is NULL, socket will be bound
 *			to any available port.
 * @param a_name	Optionally specify the published address for
 *			this transport. If the socket is not replaced
 *			(PJSIP_FAKE_UDP_TRANSPORT_KEEP_SOCKET flag is
 *			specified), then if this argument is NULL, the
 *			previous value will be used. If the socket is
 *			replaced and this argument is NULL, the bound
 *			address will be used as the published address
 *			of the transport.
 *
 * @return		PJ_SUCCESS if transport can be restarted, or
 *			the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_fake_udp_transport_restart(pjsip_transport *transport,
					         unsigned option,
						 pj_sock_t sock,
						 const pj_sockaddr_in *local,
						 const pjsip_host_port *a_name);


PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJSIP_TRANSPORT_FAKE_UDP_H__ */
