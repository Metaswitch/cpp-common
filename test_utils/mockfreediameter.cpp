/**
 * @file mockfreediameter.cpp Mock out free diameter calls.
 *
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

#include "mockfreediameter.hpp"

static MockFreeDiameter* _mock;

typedef int (*fd_msg_send_t)( struct msg ** pmsg, void (*anscb)(void *, struct msg **), void * data );
typedef int (*fd_msg_send_timeout_t)( struct msg ** pmsg, void (*anscb)(void *, struct msg **), void * data, void (*expirecb)(void *, DiamId_t, size_t, struct msg **), const struct timespec *timeout );
typedef int (*fd_msg_hdr_t)( struct msg *msg, struct msg_hdr ** pdata );
typedef int (*fd_msg_new_t)( struct dict_object * model, int flags, struct msg ** msg );
typedef int (*fd_msg_bufferize_t)( struct msg * msg, uint8_t ** buffer, size_t * len );
typedef struct fd_hook_permsgdata * (*fd_hook_get_pmd_t)(struct fd_hook_data_hdl *data_hdl, struct msg * msg);

static fd_msg_send_t real_fd_msg_send;
static fd_msg_send_timeout_t real_fd_msg_send_timeout;
static fd_msg_hdr_t real_fd_msg_hdr;
static fd_msg_new_t real_fd_msg_new;
static fd_msg_bufferize_t real_fd_msg_bufferize;
static fd_hook_get_pmd_t real_fd_hook_get_pmd;

void mock_free_diameter(MockFreeDiameter* mock)
{
  _mock = mock;
}

void unmock_free_diameter()
{
  _mock = NULL;
}

int fd_msg_send( struct msg ** pmsg, void (*anscb)(void *, struct msg **), void * data )
{
  if (real_fd_msg_send == NULL)
  {
    real_fd_msg_send = (fd_msg_send_t)dlsym(RTLD_NEXT, "fd_msg_send");
  }

  if (_mock != NULL)
  {
    return _mock->fd_msg_send(pmsg, (void*)anscb, data);
  }
  else
  {
    return real_fd_msg_send(pmsg, anscb, data);
  }
}

int fd_msg_send_timeout( struct msg ** pmsg, void (*anscb)(void *, struct msg **), void * data, void (*expirecb)(void *, DiamId_t, size_t, struct msg **), const struct timespec *timeout )
{
  if (real_fd_msg_send_timeout == NULL)
  {
    real_fd_msg_send_timeout = (fd_msg_send_timeout_t)dlsym(RTLD_NEXT, "fd_msg_send_timeout");
  }

  if (_mock != NULL)
  {
    return _mock->fd_msg_send_timeout(pmsg, (void*)anscb, data, (void*)expirecb, timeout);
  }
  else
  {
    return real_fd_msg_send_timeout(pmsg, anscb, data, expirecb, timeout);
  }
}

int fd_msg_hdr( struct msg *msg, struct msg_hdr ** pdata )
{
  if (real_fd_msg_hdr == NULL)
  {
    real_fd_msg_hdr = (fd_msg_hdr_t)dlsym(RTLD_NEXT, "fd_msg_hdr");
  }

  if (_mock != NULL)
  {
    return _mock->fd_msg_hdr(msg, pdata);
  }
  else
  {
    return real_fd_msg_hdr(msg, pdata);
  }
}

int fd_msg_new( struct dict_object * model, int flags, struct msg ** msg )
{
  if (real_fd_msg_new == NULL)
  {
    real_fd_msg_new = (fd_msg_new_t)dlsym(RTLD_NEXT, "fd_msg_new");
  }

  if (_mock != NULL)
  {
    return _mock->fd_msg_new(model, flags, msg);
  }
  else
  {
    return real_fd_msg_new(model, flags, msg);
  }
}

int fd_msg_bufferize( struct msg * msg, uint8_t ** buffer, size_t * len )
{
  if (real_fd_msg_bufferize == NULL)
  {
    real_fd_msg_bufferize = (fd_msg_bufferize_t)dlsym(RTLD_NEXT, "fd_msg_bufferize");
  }

  if (_mock != NULL)
  {
    return _mock->fd_msg_bufferize(msg, buffer, len);
  }
  else
  {
    return real_fd_msg_bufferize(msg, buffer, len);
  }
}

struct fd_hook_permsgdata * fd_hook_get_pmd(struct fd_hook_data_hdl *data_hdl, struct msg * msg)
{
  if (real_fd_hook_get_pmd == NULL)
  {
    real_fd_hook_get_pmd = (fd_hook_get_pmd_t)dlsym(RTLD_NEXT, "fd_hook_get_pmd");
  }

  printf("In mock fd_hook_get_pmd\n");

  if (_mock != NULL)
  {
    printf("Calling mock version\n");
    return _mock->fd_hook_get_pmd(data_hdl, msg);
  }
  else
  {
    printf("Calling real version\n");
    return real_fd_hook_get_pmd(data_hdl, msg);
  }
}
