/**
 * @file mockfreediameter.cpp Mock out free diameter calls.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
