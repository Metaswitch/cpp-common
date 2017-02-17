/**
 * @file bloom_filter.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2016  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include <random>
#include <cassert>

#include "bloom_filter.h"
#include "siphash.h"
#include "log.h"

#define NEXT_MULTIPLE_OF_8(X) (((X) + 7) / 8 * 8)

BloomFilter::BloomFilter(uint64_t bitmap_size, uint32_t bits_per_item) :
  _bitmap(NEXT_MULTIPLE_OF_8(bitmap_size) / 8, 0),
  _bitmap_size(bitmap_size),
  _bits_per_item(bits_per_item)
{
  TRC_DEBUG("Create bloom filter with %lu bits, %u bits per item",
            bitmap_size, bits_per_item);

  // Initialize the SIP hashers.
  std::mt19937_64 rng(time(0));
  sip_hashers[0].k0 = rng();
  sip_hashers[0].k1 = rng();
  sip_hashers[1].k0 = rng();
  sip_hashers[1].k1 = rng();

  TRC_DEBUG("SipHash keys: [(%lu,%lu), (%lu,%lu)]",
            sip_hashers[0].k0,
            sip_hashers[0].k1,
            sip_hashers[1].k0,
            sip_hashers[1].k1);
}

BloomFilter BloomFilter::for_num_entries_and_fp_prob(uint64_t num_entries,
                                                     double fp_prob)
{
  assert(fp_prob < 1.0);
  assert(fp_prob > 0.0);

  uint32_t bits_per_item = ceil(-log(fp_prob) / log(2));

  num_entries = std::max(num_entries, 1ul);

  double factor = -1.0 * log(fp_prob) / (log(2) * log(2));
  uint64_t bitmap_size = num_entries * factor;

  return BloomFilter(bitmap_size, bits_per_item);
}

void BloomFilter::add(const std::string& item)
{
  TRC_DEBUG("Add %s to the bloom filter", item.c_str());

  std::vector<uint64_t> hash_values = calculate_hash_values(item);

  for (uint64_t h: hash_values)
  {
    set_bit(h % _bitmap_size);
  }
}

bool BloomFilter::check(const std::string& item)
{
  bool present = true;

  std::vector<uint64_t> hash_values = calculate_hash_values(item);

  for (uint64_t h: hash_values)
  {
    // If any of the required bits are not set, then this item definitely isn't
    // in the bloom filter.
    if (!is_bit_set(h % _bitmap_size))
    {
      present = false;
      break;
    }
  }

  TRC_DEBUG("%s is %sin bloom filter", item.c_str(), present ? "" : "not ");
  return present;
}

uint64_t BloomFilter::calculate_sip_hash_value(const SipHashKeys& keys,
                                               const std::string& item)
{
  uint8_t key[16];
  *((uint64_t*)key) = keys.k0;
  *((uint64_t*)(key + 8)) = keys.k1;

  uint64_t hash_value;

  siphash((uint8_t*)item.c_str(),
          item.length(),
          key,
          (uint8_t*)&hash_value,
          sizeof(hash_value));

  return hash_value;
}

std::vector<uint64_t> BloomFilter::calculate_hash_values(const std::string& item)
{
  TRC_DEBUG("Calculate hash values for %s", item.c_str());

  std::vector<uint64_t> hash_values(_bits_per_item);

  // The first two hash values are calculated by taking a SIP hash of the item.
  // Subsequent hash values are formed from a linear combination of the first
  // two hash values. This means we only ever perform two hashes, regardless of
  // the number of bits per entry, which is good for performance.
  for (uint32_t ii = 0; ii < _bits_per_item; ++ii)
  {
    if (ii < 2)
    {
      hash_values[ii] = calculate_sip_hash_value(sip_hashers[ii], item);
    }
    else
    {
      hash_values[ii] = hash_values[0] + hash_values[1] * ii;
    }

    TRC_DEBUG("Hash value #%d = %lu", ii, hash_values[ii]);
  }

  return hash_values;
}

bool BloomFilter::is_bit_set(uint64_t bit)
{
  TRC_DEBUG("Checking bit %lu", bit);

  uint64_t byte_index = bit / 8;
  uint32_t bit_index = 7 - (bit % 8);

  return ((_bitmap[byte_index] & (0x01 << bit_index)) != 0);
}

void BloomFilter::set_bit(uint64_t bit)
{
  TRC_DEBUG("Setting bit %lu", bit);

  uint64_t byte_index = bit / 8;
  uint32_t bit_index = 7 - (bit % 8);

  _bitmap[byte_index] |= (0x01 << bit_index);
}
