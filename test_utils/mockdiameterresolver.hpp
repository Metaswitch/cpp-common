/**
 * @file mockdiameterresolver.hpp Mock Diameter resolver.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCKDIAMETERRESOLVER_H__
#define MOCKDIAMETERRESOLVER_H__

#include "gmock/gmock.h"
#include "diameterresolver.h"

class MockDiameterResolver : public DiameterResolver
{
public:
  MockDiameterResolver() :
    DiameterResolver(NULL, AF_INET)
  {};
  virtual ~MockDiameterResolver() {};

  MOCK_METHOD5(resolve, void(const std::string&,
                             const std::string&,
                             int,
                             std::vector<AddrInfo>&,
                             int&));
};

#endif
