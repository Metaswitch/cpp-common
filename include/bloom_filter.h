/**
 * @file bloom_filter.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef BLOOM_FILTER_CPP__
#define BLOOM_FILTER_CPP__

#include "rapidjson/writer.h"
#include "rapidjson/document.h"

#include <vector>

class BloomFilter
{
public:
  /// Create a bloom filter by specifying the total bitmap size and the number
  /// of bits per key.
  ///
  /// @param bitmap_size  - the total size of the bitmap.
  /// @param bit_per_item - the number of bits that are used to store each item.
  BloomFilter(uint64_t bitmap_size, uint32_t bits_per_item);

  /// Create a bloom filter for a given number of entries with a particular
  /// false positive probability.
  ///
  /// @param num_entries - The number of entries expected in the bloom filter.
  ///                      Must be in the range 0.0 - 1.0 (not inclusive).
  /// @param fp_prob     - The false positive probability for the filter.
  ///                      Must be > 0.
  /// @return            - The constructed bloom filter, or nullptr if the
  ///                      arguments were unacceptable.
  static BloomFilter* for_num_entries_and_fp_prob(uint64_t num_entries,
                                                  double fp_prob);

  /// Construct a bloom filter from a JSON value.
  ///
  /// @param json - The JSON value in question.
  /// @return     - The constructed bloom filter, or nullptr if the JSON was
  ///               semantically invalid.
  static BloomFilter* from_json(const std::string& json);

  /// Add an item to the bloom filter.
  ///
  /// @param item - The item to set.
  void add(const std::string& item);

  /// Check whether item is present in the bloom filter.
  ///
  /// @param key - The item to check.
  /// @return    - False if the item is not present. True if it *might* be
  ///              present (bloom filters can give false positives)
  bool check(const std::string& item);

  /// Serialize the bloom filter to JSON.
  ///
  /// @return - The json in string form.
  std::string to_json();

protected:
  // Make the default constructor protected so that users can't default
  // construct a bloom filter, but the alternative constructors can construct an
  // empty bloom filter and fill it in.
  BloomFilter();

private:
  // The underlying bitmap that the bloom filter uses to store its data. This
  // is arranged so that the 0th bit is the highest order bit in the 0th byte.
  std::string _bitmap;

  // The number of valid bits in the above bitmap. This is stored as a separate
  // variable in case the bitmap needs to contain a number of bits that is not
  // a multiple of 8.
  uint64_t _bitmap_size;

  // The number of bits for each item.
  uint32_t _bits_per_item;

  // This bloom filter uses two independent SIP hashers. Each one is described
  // by a pair of 64-bit integer keys - k0 and k1.
  struct SipHashKeys
  {
    uint64_t k0;
    uint64_t k1;
  };

  SipHashKeys sip_hashers[2];

  // Calculate the SIP hash of a given item
  //
  // @param keys - The keys to initialize the SIP hasher with.
  // @param item - The item to take the hash of.
  //
  // @return     - The computed hash value.
  uint64_t calculate_sip_hash_value(const SipHashKeys& keys,
                                    const std::string& item);

  // Calculate the hash values for a given item. This returns a vector
  // containing `_bits_per_item` values.
  std::vector<uint64_t> calculate_hash_values(const std::string& item);

  // Utility function to check whether a bit is set in the bitmap.
  // @param bit - the index of the bit to check.
  // @return    - whether the bit is set or not.
  bool is_bit_set(uint64_t bit);

  // Utility function to set a bit in the bitmap.
  // @param bit - the index of the bit to set.
  void set_bit(uint64_t bit);

  // Utility function to write a Sip hasher out as a JSON object.
  //
  // @param hasher - The hasher in question.
  // @param writer - A rapidjson writer to write to.
  void sip_hash_to_json(const SipHashKeys& hasher,
                        rapidjson::Writer<rapidjson::StringBuffer>& writer);

  // Utility function to read a Sip Hasher from a JSON value.
  //
  // @param json_val - The value to read from.
  // @param hasher   - The hasher to read into.
  static void sip_hash_from_json(const rapidjson::Value& json_val,
                                 SipHashKeys& hasher);
};

#endif
