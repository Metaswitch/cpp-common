/**
 * @file fake_base_addr_iterator.h Fake BaseAddrIterator for testing.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "baseresolver.h"

class FakeBaseAddrIterator : public BaseAddrIterator
{
public:
  FakeBaseAddrIterator(AddrInfo target) : _target(target) {}
  virtual ~FakeBaseAddrIterator() {}

  virtual std::vector<AddrInfo> take(int num_requested_targets)
  {
    std::vector<AddrInfo>  v;
    v.push_back(_target);
    return v;
  }

private:
  AddrInfo _target;
};

