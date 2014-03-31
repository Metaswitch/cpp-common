/**
 * @file random_uuid.h Random UUID generator
 *
 * project clearwater - ims in the cloud
 * copyright (c) 2013  metaswitch networks ltd
 *
 * this program is free software: you can redistribute it and/or modify it
 * under the terms of the gnu general public license as published by the
 * free software foundation, either version 3 of the license, or (at your
 * option) any later version, along with the "special exception" for use of
 * the program along with ssl, set forth below. this program is distributed
 * in the hope that it will be useful, but without any warranty;
 * without even the implied warranty of merchantability or fitness for
 * a particular purpose.  see the gnu general public license for more
 * details. you should have received a copy of the gnu general public
 * license along with this program.  if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * the author can be reached by email at clearwater@metaswitch.com or by
 * post at metaswitch networks ltd, 100 church st, enfield en2 6bq, uk
 *
 * special exception
 * metaswitch networks ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining openssl with the
 * software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the gpl. you must comply with the gpl in all
 * respects for all of the code used other than openssl.
 * "openssl" means openssl toolkit software distributed by the openssl
 * project and licensed under the openssl licenses, or a work based on such
 * software and licensed under the openssl licenses.
 * "openssl licenses" means the openssl license and original ssleay license
 * under which the openssl project distributes the openssl toolkit software,
 * as those licenses appear in the file license-openssl.
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
