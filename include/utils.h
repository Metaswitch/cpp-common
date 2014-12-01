/**
 * @file utils.h Utility functions.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
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

#ifndef UTILS_H_
#define UTILS_H_

#include <math.h>
#include <algorithm>
#include <functional>
#include <string>
#include <list>
#include <vector>
#include <cctype>
#include <string.h>
#include <arpa/inet.h>

#include "log.h"

struct IP46Address
{
  int af;
  union
  {
    struct in_addr ipv4;
    struct in6_addr ipv6;
  } addr;

  int compare(const IP46Address& rhs) const
  {
    if (af != rhs.af)
    {
      return af - rhs.af;
    }
    else if (af == AF_INET)
    {
      return addr.ipv4.s_addr - rhs.addr.ipv4.s_addr;
    }
    else if (af == AF_INET6)
    {
      return memcmp((const char*)&addr.ipv6, (const char*)&rhs.addr.ipv6, sizeof(in6_addr));
    }
    else
    {
      return false;
    }
  }
};

namespace Utils
{
  static const int MD5_HASH_SIZE = 16;
  static const int HEX_HASH_SIZE = 32;

  static const char _b64[64] =
  {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
  };

  void hashToHex(unsigned char *hash_char, unsigned char *hex_char);

  std::string url_unescape(const std::string& s);
  std::string url_escape(const std::string& s);

  std::string xml_escape(const std::string& s);

  std::string ip_addr_to_arpa(IP46Address ip_addr);

  void create_random_token(size_t length, std::string& token);

  // trim from start
  inline std::string& ltrim(std::string &s)
  {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                    std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
  }

  // trim from end
  inline std::string& rtrim(std::string &s)
  {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
  }

  // trim from both ends
  inline std::string& trim(std::string &s)
  {
    return ltrim(rtrim(s));
  }

  // Helper function to prevent split_string from splitting on the
  // delimiter if it is enclosed by quotes
  inline size_t find_unquoted(std::string str, char c, size_t offset = 0)
  {
    const char quoter = '"';
    size_t quote = str.find(quoter, offset);
    size_t next_quote = str.find(quoter, quote + 1);
    size_t pos = str.find(c, offset);

    while (pos != std::string::npos)
    {
      if ((quote == std::string::npos) || (pos < quote))
      {
        // No more quotes
        return pos;
      }
      else if (pos > quote)
      {
        // Character appears after first quote
        if (next_quote == std::string::npos)
        {
          // There is no closing quote - call that not found
          return std::string::npos;
        }
        else if (pos > next_quote)
        {
          // The character is not within these quotes
          // Go again with updated quotes (not updated pos)
          quote = str.find(quoter, next_quote + 1);
          next_quote = str.find(quoter, quote + 1);
        }
        else
        {
          // Character is quoted. Try to find another
          pos = str.find(c, next_quote + 1);
          quote = str.find(quoter, next_quote + 1);
          next_quote = str.find(quoter, quote + 1);
        }
      }
    }
    return std::string::npos;
  }

  /// Split the string s using delimiter and store the resulting tokens in order
  /// at the end of tokens. Only non-empty tokens will be stored; empty tokens are ignored (and not counted).
  template <class T>  //< container that has T::push_back(std::string)
  void split_string(const std::string& str_in,  //< string to scan (will not be changed)
                    char delimiter,  //< delimiter to use
                    T& tokens,  //< tokens will be added to this list
                    const int max_tokens = 0,  //< max number of tokens to push; last token will be tail of string (delimiters will not be parsed in this section)
                    bool trim = false,  //< trim the string at both ends before splitting?
                    bool check_for_quotes = false) //< only use delimiters not in quotes
  {
    std::string token;

    std::string s = str_in;
    if (trim)
    {
      Utils::trim(s);
    }

    size_t token_start_pos = 0;
    size_t token_end_pos;
    if (check_for_quotes)
    {
      token_end_pos = Utils::find_unquoted(s, delimiter);
    }
    else
    {
      token_end_pos = s.find(delimiter);
    }

    int num_tokens = 0;

    while ((token_end_pos != std::string::npos) &&
           ((max_tokens == 0) ||
            (num_tokens < (max_tokens-1))))
    {
      token = s.substr(token_start_pos, token_end_pos - token_start_pos);
      if (token.length() > 0)
      {
        tokens.push_back(token);
        num_tokens++;
      }
      token_start_pos = token_end_pos + 1;
      if (check_for_quotes)
      {
        token_end_pos = Utils::find_unquoted(s, delimiter, token_start_pos);
      }
      else
      {
        token_end_pos = s.find(delimiter, token_start_pos);
      }
    }

    token = s.substr(token_start_pos);
    if (token.length() > 0)
    {
      tokens.push_back(token);
    }
  }

  /// Generates a random number which is exponentially distributed
  class ExponentialDistribution
  {
  public:
    ExponentialDistribution(double lambda) :
      _lambda(lambda)
    {
    }

    ~ExponentialDistribution()
    {
    }

    int operator() ()
    {
      // Generate a uniform random number in the range [0,1] then transform
      // it to an exponentially distributed number using a formula for the
      // inverted CDF.
      double r = (double)rand() / (double)RAND_MAX;
      return -log(r)/_lambda;
    }

  private:
    double _lambda;
  };

  /// Generates a random number which is binomially distributed
  class BinomialDistribution
  {
  public:
    BinomialDistribution(int t, double p) :
      _cdf(t + 1)
    {
      // Compute the discrete CDF for the distribution using the recurrence
      // relation for the PDF
      //    PDF(i) = PDF(i-1) * ((t-i+1)/i) * (p/(1-p))
      double pr = p / (1 - p);
      double pdf = pow(1 - p, t);
      _cdf[0] = pdf;
      for (int i = 1; i <= t; ++i)
      {
        pdf *= pr * (double)(t - i + 1)/(double)i;
        _cdf[i] = _cdf[i-1] + pdf;
      }
    }

    ~BinomialDistribution()
    {
    }

    int operator() ()
    {
      // Find the lower bound in the CDF
      double r = (double)rand() / (double)RAND_MAX;
      std::vector<double>::iterator i = lower_bound(_cdf.begin(), _cdf.end(), r);
      return i - _cdf.begin();
    }

  private:
    std::vector<double> _cdf;
  };

  /// Measures time delay in microseconds
  class StopWatch
  {
  public:
    inline StopWatch() : _ok(true), _running(true), _elapsed_us(0) {}

    /// Starts the stop-watch, returning whether it was successful.  It's OK
    /// to ignore the return code - it will also be returned on read() and
    /// stop().
    inline bool start()
    {
      _ok = (clock_gettime(CLOCK_MONOTONIC, &_start) == 0);

      if (_ok)
      {
        _running = true;
      }
      else if (!_already_logged)
      {
        LOG_ERROR("Failed to get start timestamp: %s", strerror(errno));
        _already_logged = true;
      }

      return _ok;
    }

    /// Stops the stop-watch, returning whether it was successful.  The recorded
    /// time is stored internal and can be read by a subsequent call to read().
    /// It's OK to ignore the return code to stop() - it will also be returned
    /// on read().
    inline bool stop()
    {
      if (_running)
      {
        _ok = read(_elapsed_us);
        _running = false;
      }

      return _ok;
    }

    /// Reads the stopwatch (which does not have to be stopped) and returns
    /// whether this was successful. result_us is not valid on failure.
    inline bool read(unsigned long& result_us)
    {
      if (!_ok)
      {
        return _ok;
      }

      if (_running)
      {
        struct timespec now;
        _ok = (clock_gettime(CLOCK_MONOTONIC, &now) == 0);

        if (_ok)
        {
          result_us = (now.tv_nsec - _start.tv_nsec) / 1000L +
                      (now.tv_sec - _start.tv_sec) * 1000000L;
        }
        else
        {
          LOG_ERROR("Failed to get stop timestamp: %s", strerror(errno));
        }
      }
      else
      {
        result_us = _elapsed_us;
      }

      return _ok;
    }

  private:
    struct timespec _start;
    bool _ok;
    bool _running;
    unsigned long _elapsed_us;

    static bool _already_logged;
  };

  // Unique number generator.  Uses the current timestamp to generate deployment
  // wide numbers that are guaranteed to be unique for ~8 years.
  //
  // @param A 3-bit identifier for the deployment.
  // @param A 7-bit identifier for the instance.
  //
  // @return A 64-bit identifier
  uint64_t generate_unique_integer(uint32_t deployment_id, uint32_t instance_id);

} // namespace Utils

#endif /* UTILS_H_ */
