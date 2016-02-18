/**
 * @file mockfreediameter.hpp Mock out freeDiameter calls.
 * project clearwater - ims in the cloud
 * copyright (c) 2013  metaswitch networks ltd
 *
 * this program is free software: you can redistribute it and/or modify it
 * under the terms of the gnu general public license as published by the
 * free software foundation, either version 3 of the license, or (at your
 * option) any later version, along with the "special exception" for use of
 * the program along with ssl, set forth below. this program is distributed
 * in the hope that it will be useful, but without any warranty;
 * without even the implied warranty of merchantability or fitness for
 * a particular purpose.  see the gnu general public license for more
 * details. you should have received a copy of the gnu general public
 * license along with this program.  if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * the author can be reached by email at clearwater@metaswitch.com or by
 * post at metaswitch networks ltd, 100 church st, enfield en2 6bq, uk
 *
 * special exception
 * metaswitch networks ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining openssl with the
 * software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the gpl. you must comply with the gpl in all
 * respects for all of the code used other than openssl.
 * "openssl" means openssl toolkit software distributed by the openssl
 * project and licensed under the openssl licenses, or a work based on such
 * software and licensed under the openssl licenses.
 * "openssl licenses" means the openssl license and original ssleay license
 * under which the openssl project distributes the openssl toolkit software,
 * as those licenses appear in the file license-openssl.
 */

#ifndef MOCKFREEDIAMATER_HPP__
#define MOCKFREEDIAMATER_HPP__

#include <dlfcn.h>

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>

#include "gmock/gmock.h"

class MockFreeDiameter
{
public:
  MOCK_METHOD3(fd_msg_send, int( struct msg ** pmsg, void* anscb, void * data ));
  MOCK_METHOD5(fd_msg_send_timeout, int( struct msg ** pmsg, void *anscb, void * data, void *expirecb, const struct timespec *timeout ));
  MOCK_METHOD2(fd_msg_hdr, int( struct msg *msg, struct msg_hdr ** pdata ));
  MOCK_METHOD3(fd_msg_new, int( struct dict_object * model, int flags, struct msg ** msg ));
  MOCK_METHOD3(fd_msg_bufferize, int( struct msg * msg, uint8_t ** buffer, size_t * len ));
  MOCK_METHOD2(fd_hook_get_pmd, struct fd_hook_permsgdata * (struct fd_hook_data_hdl *data_hdl, struct msg * msg));

  struct msg_hdr hdr;

  MockFreeDiameter()
  {
    // Initialize the message header to avoid valgrind errors.
    memset(&hdr, 0, sizeof(hdr));
  }
};

void mock_free_diameter(MockFreeDiameter* mock);
void unmock_free_diameter();

#endif

