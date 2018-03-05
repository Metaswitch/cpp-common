/**
 * @file bloom_filter.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <random>
#include <cassert>
#include <memory>

#include "bloom_filter.h"
#include "siphash.h"
#include "log.h"
#include "json_parse_utils.h"
#include "rapidjson/error/en.h"
#include "base64.h"

// Constants used in JSON serialization.
static const char* JSON_BITMAP = "bitmap";
static const char* JSON_TOTAL_BITS = "total_bits";
static const char* JSON_BITS_PER_ENTRY = "bits_per_entry";
static const char* JSON_HASH0 = "hash0";
static const char* JSON_HASH1 = "hash1";
static const char* JSON_K0 = "k0";
static const char* JSON_K1 = "k1";

// Macro that works out how many bytes are needed to store a given number of
// bits.
#define NUM_BYTES_FOR_BITS(X) (((X) + 7) / 8)

BloomFilter::BloomFilter() {}

BloomFilter::BloomFilter(uint64_t bitmap_size, uint32_t bits_per_item) :
  _bitmap(NUM_BYTES_FOR_BITS(bitmap_size), 0),
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

BloomFilter* BloomFilter::for_num_entries_and_fp_prob(uint64_t num_entries,
                                                     double fp_prob)
{
  // Check that the inputs to the function are acceptable.
  if ((fp_prob <= 0.0) || (fp_prob >= 1.0))
  {
    TRC_WARNING("Bad bloom filter false positive probability %f, "
                "must be in range (0.0, 1.0)",
                fp_prob);
    return nullptr;
  }

  if (num_entries <= 0)
  {
    TRC_WARNING("Bad bloom filter number of entries %lu, must be >0",
                num_entries);
    return nullptr;
  }

  // Work out how many bits we need per item, and how big the bitmap should be.
  // The optimal values are taken from https://en.wikipedia.org/wiki/Bloom_filter
  uint32_t bits_per_item = ceil(-log(fp_prob) / log(2));

  double factor = -1.0 * log(fp_prob) / (log(2) * log(2));
  uint64_t bitmap_size = num_entries * factor;

  return new BloomFilter(bitmap_size, bits_per_item);
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
  // The reference C implementation of SipHash takes a 16-byte array. Form this
  // by concatenating the two keys with native byte ordering.
  uint8_t key[16];
  memcpy(key, &keys.k0, 8);
  memcpy(key + 8, &keys.k1, 8);

  // Similarly the result is returned as a 8-byte array. Interpret this as a
  // 64-bit uint in native byte ordering.
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

std::string BloomFilter::to_json()
{
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

  writer.StartObject();
  {
    writer.String(JSON_BITMAP);
    std::string bitmap = base64_encode(_bitmap);
    writer.String(bitmap.c_str());

    writer.String(JSON_TOTAL_BITS); writer.Uint64(_bitmap_size);
    writer.String(JSON_BITS_PER_ENTRY); writer.Uint(_bits_per_item);
    writer.String(JSON_HASH0); sip_hash_to_json(sip_hashers[0], writer);
    writer.String(JSON_HASH1); sip_hash_to_json(sip_hashers[1], writer);
  }
  writer.EndObject();

  return sb.GetString();
}

void BloomFilter::sip_hash_to_json(const SipHashKeys& hasher,
                                   rapidjson::Writer<rapidjson::StringBuffer>& writer)
{
  writer.StartObject();
  {
    writer.String(JSON_K0); writer.Uint64(hasher.k0);
    writer.String(JSON_K1); writer.Uint64(hasher.k1);
  }
  writer.EndObject();
}

BloomFilter* BloomFilter::from_json(const std::string& json)
{
  rapidjson::Document doc;
  doc.Parse<0>(json.c_str());

  if (doc.HasParseError())
  {
    TRC_DEBUG("Failed to parse document: %s\nError: %s",
              json.c_str(),
              rapidjson::GetParseError_En(doc.GetParseError()));
    return nullptr;
  }

  // The string we've been passed is valid JSON. GO ahead and parse it. Use a
  // unique pointer to we can return early without having to remember to free
  // it.
  std::unique_ptr<BloomFilter> filter(new BloomFilter());

  try
  {
    JSON_ASSERT_OBJECT(doc);

    // Start by extracting the bitmap. Extract it to a temporary variable, and
    // check it is valid base64, before decoding it and storing it back in the
    // filter.
    std::string bitmap_base64;
    JSON_GET_STRING_MEMBER(doc, JSON_BITMAP, bitmap_base64);

    if (!is_base64(bitmap_base64))
    {
      TRC_INFO("Invalid base64");
      return nullptr;
    }

    filter->_bitmap = base64_decode(bitmap_base64);

    // Get the remaining trivial members.
    JSON_GET_UINT_64_MEMBER(doc, JSON_TOTAL_BITS, filter->_bitmap_size);
    JSON_GET_UINT_MEMBER(doc, JSON_BITS_PER_ENTRY, filter->_bits_per_item);
    JSON_ASSERT_CONTAINS(doc, JSON_HASH0); sip_hash_from_json(doc[JSON_HASH0],
                                                              filter->sip_hashers[0]);
    JSON_ASSERT_CONTAINS(doc, JSON_HASH1); sip_hash_from_json(doc[JSON_HASH1],
                                                              filter->sip_hashers[1]);
  }
  catch(JsonFormatError err)
  {
    TRC_INFO("Failed to deserialize JSON document (hit error at %s:%d)",
             err._file, err._line);
    return nullptr;
  }

  return filter.release();
}

void BloomFilter::sip_hash_from_json(const rapidjson::Value& json_val,
                                     SipHashKeys& hasher)
{
  JSON_GET_UINT_64_MEMBER(json_val, JSON_K0, hasher.k0);
  JSON_GET_UINT_64_MEMBER(json_val, JSON_K1, hasher.k1);
}
