/**
 * Project Clearwater - IMS in the Cloud
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

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <string>

#include "namespace_hop.h"
#include "log.h"

// Send a message with 'control messages (also called ancillary data) that are not a part of the
// socket payload'. The control messages can convey a file descriptor as the SCM_RIGHTS - this is
// the actual data we want to receive over this socket.
static int recv_file_descriptor(int socket)
{
  // Buffer to hold the dummy "actual data" we receive
  char data[1] = {0};

  struct iovec iov[1];
  iov[0].iov_base = data;
  iov[0].iov_len = sizeof(data);

  // Buffer to hold the control messages we receive
  char ctrl_buf[CMSG_SPACE(sizeof(int))] = {0};

  // Message struct wrapping both buffers.
  struct msghdr message = {0};
  message.msg_name = NULL;
  message.msg_namelen = 0;
  message.msg_control = ctrl_buf;
  message.msg_controllen = CMSG_SPACE(sizeof(int));
  message.msg_iov = iov;
  message.msg_iovlen = 1;

  int res = ::recvmsg(socket, &message, 0);
  if (res <= 0)
  {
    TRC_WARNING("Failed to retrieve cross-namespace socket - recvmsg returned %d (%d %s)\n", res, errno, strerror(errno));
    return -1;
  }

  // Iterate through control message to find if there is a SCM_RIGHTS entry containing file descriptors
  struct cmsghdr *control_message = NULL;

  for (control_message = CMSG_FIRSTHDR(&message);
       control_message != NULL;
       control_message = CMSG_NXTHDR(&message, control_message))
  {
    if ((control_message->cmsg_level == SOL_SOCKET) &&
        (control_message->cmsg_type == SCM_RIGHTS))
    {
      return *((int*)CMSG_DATA(control_message));
    }
  }

  TRC_ERROR("No cross-namespace socket received\n");
  return -1;
}

// Communicate with one of the clearwater-socket-factory-owned UNIX sockets to retrieve a file
// descriptor in another namespace.
//
// The API here is:
//  - the client sends the string "host:port" over the UNIX domain socket
//  - clearwater-socket-factory opens a TCP socket to that host and port in the appropriate
//  namespace
//  - clearwater-socket-factory sends a message back over the UNIX domain socket, with the file
//  descriptor of that TCP socket as ancillary data
//  - the client can now use that file descriptor to communicate outside its network namespace
int create_connection_in_namespace(const char* host,
                                   const char* port,
                                   const char* socket_factory_path)
{
  std::string target = (host + std::string(":") + port);

  TRC_DEBUG("Get cross-namespace socket to %s via %s\n",
            target.c_str(),
            socket_factory_path);
  
  struct sockaddr_un addr = {AF_LOCAL};
  strcpy(addr.sun_path, socket_factory_path);
  int fd = socket(AF_LOCAL, SOCK_STREAM, 0);

  if (fd < 0)
  {
    TRC_ERROR("Failed to create client socket to cross-namespace socket factory");
    return fd;
  }

  int ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (ret < 0)
  {
    TRC_ERROR("Failed to connect to cross-namespace socket factory %s",
              socket_factory_path);
    return -1;
  }


  ret = send(fd, target.c_str(), target.size(), 0);
  if (ret < 0)
  {
    TRC_ERROR("Error sending target '%s' to %s: %s",
              target.c_str(),
              socket_factory_path,
              strerror(errno));
    return -2;
  }

  return recv_file_descriptor(fd);
}


int create_connection_in_signaling_namespace(const char* host,
                                             const char* port)
{
  return create_connection_in_namespace(host,
                                        port,
                                        "/tmp/clearwater_signaling_namespace_socket");
}

int create_connection_in_management_namespace(const char* host,
                                              const char* port)
{
  return create_connection_in_namespace(host,
                                        port,
                                        "/tmp/clearwater_management_namespace_socket");
}
