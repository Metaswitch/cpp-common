/**
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef NAMESPACE_HOP_H
#define NAMESPACE_HOP_H

#ifdef __cplusplus
extern "C" {
#endif

int create_connection_in_signaling_namespace(const char* host, const char* port);
int create_connection_in_management_namespace(const char* host, const char* port);

#ifdef __cplusplus
}
#endif

#endif
