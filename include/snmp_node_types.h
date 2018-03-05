/**
 * @file snmp_node_types.h
 *
 * Copyright (C) Metaswitch Networks 2015
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef SNMP_NODE_TYPES_H
#define SNMP_NODE_TYPES_H

namespace SNMP
{

enum NodeTypes
{
  SCSCF = 0,
  PCSCF = 1,
  ICSCF = 2,
  MRFC = 3, 
  MGCF = 4, 
  BGCF = 5,
  AS = 6,
  IBCF = 7,
  SGW = 8,
  PGW = 9,
  HSGW = 10,
  ECSCF = 11, 
  MME = 12, 
  TRF = 13, 
  TF = 14,
  ATCF = 15,
  PROXYFUNCTION = 16,
  EPDG = 17
};
}

#endif
