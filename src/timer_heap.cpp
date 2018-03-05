/**
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "timer_heap.h"
#include "utils.h"

bool PopsBefore::operator()(HeapableTimer* const& t1, HeapableTimer* const& t2) const
{
  return Utils::overflow_less_than(t2->get_pop_time(), t1->get_pop_time());
}


