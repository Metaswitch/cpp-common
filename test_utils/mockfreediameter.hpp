/**
 * @file mockfreediameter.hpp Mock out freeDiameter calls.
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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

