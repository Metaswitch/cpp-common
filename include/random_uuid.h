/**
 * @file random_uuid.h Random UUID generator
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef RANDOM_UUID_H__
#define RANDOM_UUID_H__

#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <fstream>

/// Generator of random UUIDs.
///
/// This is a wrapper around the boost::uuids::random_generator, except that we
/// explicitly use a pseudo random number generator (prng) that is just seeded
/// from /dev/urandom as opposed to the default boost implementation that uses
/// uninitialzed memory (and causes valgrind errors).
///
/// This class has the same threadsafety as boost::uuids::random_generator. TO
/// quote from
/// http://www.boost.org/doc/libs/1_53_0/libs/uuid/uuid.html#Design%20notes
///
/// "All functions are re-entrant. Classes are as thread-safe as an int. That is
/// an instance can not be shared between threads without proper
/// synchronization."
class RandomUUIDGenerator
{
public:
  RandomUUIDGenerator() : _prng()
  {
    // Get a seed value by reading from /dev/urandom.
    uint32_t seed;
    std::ifstream fin("/dev/urandom", std::ifstream::binary);
    fin.read((char*)&seed, sizeof(seed));

    // See the prng and create the boost UUID generator.
    _prng.seed(seed);
    _gen = new boost::uuids::random_generator(_prng);
  }

  ~RandomUUIDGenerator()
  {
    delete _gen; _gen = NULL;
  }

  /// Create a random UUID.
  /// @return the UUID created.
  boost::uuids::uuid operator() ()
  {
    return (*_gen)();
  }

private:
  boost::mt19937 _prng;
  boost::uuids::random_generator* _gen;
};

#endif
