/**
 * @file snmp_agent.h Initialization and termination functions for Sprout SNMP.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef CW_SNMP_AGENT_H
#define CW_SNMP_AGENT_H

// Starts the SNMP agent thread. 'name' is passed through to the netsnmp library as the application
// name - this is arbitrary, but should be spomething sensible (e.g. 'sprout', 'bono').
int snmp_setup(const char* name);

int init_snmp_handler_threads(const char* name);

// Terminates the SNMP agent thread. 'name' should match the string passed to snmp_setup.
void snmp_terminate(const char* name);

#endif
