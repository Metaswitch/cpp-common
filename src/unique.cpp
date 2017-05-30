/**
 * Copyright (C) Metaswitch Networks 2014
 * If license terms are provided to you in a COPYING file in the root
 * directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "utils.h"

#include <atomic>
#include <time.h>

namespace Utils {

uint64_t generate_unique_integer(uint32_t deployment_id, uint32_t instance_id)
{
  static const uint32_t instance_id_bits = 7;
  static const uint32_t deployment_id_bits = 3;
  static const uint32_t sequence_bits = 20;

  static const uint32_t instance_id_shift = sequence_bits;
  static const uint32_t deployment_id_shift = instance_id_shift + instance_id_bits;
  static const uint32_t timestamp_shift = deployment_id_shift + deployment_id_bits;

  static const uint32_t sequence_mask = 0xFFFFFFFF ^ (0xFFFFFFFF << sequence_bits);
  static std::atomic<uint32_t> sequence_number(0);

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t timestamp = ((uint64_t)ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
  uint32_t local_sequence_number = sequence_number++;

  uint64_t rc = (timestamp << timestamp_shift) |
                (deployment_id << deployment_id_shift) |
                (instance_id << instance_id_shift) |
                (local_sequence_number & sequence_mask);
  return rc;
}

}
