/**
 * @file utils.cpp Utility functions.
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

///

// Common STL includes.
#include <cassert>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <queue>
#include <string>
#include <arpa/inet.h>

#include "utils.h"

#define REPLACE(CHAR1, CHAR2, RESULT) if ((s[(ii+1)] == CHAR1) && (s[(ii+2)] == CHAR2)) { r.append(RESULT); ii = ii+2; continue; }

std::string Utils::url_unescape(const std::string& s)
{
  std::string r;
  r.reserve(s.length());  // Reserve enough space to avoid continually reallocating.

  for (size_t ii = 0; ii < s.length(); ++ii)
  {
    if (((ii + 2) < s.length()) && (s[ii] == '%'))
    {
      // The following characters are reserved, so must be percent-encoded per http://en.wikipedia.org/wiki/Percent-encoding#Percent-encoding_reserved_characters
      REPLACE('2', '1', "!");
      REPLACE('2', '3', "#");
      REPLACE('2', '4', "$");
      REPLACE('2', '6', "&");
      REPLACE('2', '7', "'");
      REPLACE('2', '8', "(");
      REPLACE('2', '9', ")");
      REPLACE('2', 'A', "*");
      REPLACE('2', 'B', "+");
      REPLACE('2', 'C', ",");
      REPLACE('2', 'F', "/");
      REPLACE('3', 'A', ":");
      REPLACE('3', 'B', ";");
      REPLACE('3', 'D', "=");
      REPLACE('3', 'F', "?");
      REPLACE('4', '0', "@");
      REPLACE('5', 'B', "[");
      REPLACE('5', 'D', "]");

      // The following characters are commonly percent-encoded per http://en.wikipedia.org/wiki/Percent-encoding#Character_data
      REPLACE('2', '0', " ");
      REPLACE('2', '2', "\"");
      REPLACE('2', '5', "%");
      REPLACE('2', 'D', "-");
      REPLACE('2', 'E', ".");
      REPLACE('3', 'C', "<");
      REPLACE('3', 'E', ">");
      REPLACE('5', 'C', "\\");
      REPLACE('5', 'E', "^");
      REPLACE('5', 'F', "_");
      REPLACE('6', '0', "`");
      REPLACE('7', 'B', "{");
      REPLACE('7', 'C', "|");
      REPLACE('7', 'D', "}");
      REPLACE('7', 'E', "~");

    }
    r.push_back(s[ii]);
  }
  return r;
}


std::string Utils::url_escape(const std::string& s)
{
  std::string r;
  r.reserve(2*s.length());  // Reserve enough space to avoid continually reallocating.

  for (size_t ii = 0; ii < s.length(); ++ii)
  {
    switch (s[ii])
    {
      // The following characters are reserved, so must be percent-encoded per http://en.wikipedia.org/wiki/Percent-encoding#Percent-encoding_reserved_characters
      case 0x21: r.append("%21"); break; // !
      case 0x23: r.append("%23"); break; // #
      case 0x24: r.append("%24"); break; // $
      case 0x25: r.append("%25"); break; // %
      case 0x26: r.append("%26"); break; // &
      case 0x27: r.append("%27"); break; // '
      case 0x28: r.append("%28"); break; // (
      case 0x29: r.append("%29"); break; // )
      case 0x2a: r.append("%2A"); break; // *
      case 0x2b: r.append("%2B"); break; // +
      case 0x2c: r.append("%2C"); break; // ,
      case 0x2f: r.append("%2F"); break; // forward slash
      case 0x3a: r.append("%3A"); break; // :
      case 0x3b: r.append("%3B"); break; // ;
      case 0x3d: r.append("%3D"); break; // =
      case 0x3f: r.append("%3F"); break; // ?
      case 0x40: r.append("%40"); break; // @
      case 0x5b: r.append("%5B"); break; // [
      case 0x5d: r.append("%5D"); break; // ]

      // We don't have to percent-encode these characters, but it's
      // common to do so
      case 0x20: r.append("%20"); break; // space
      case 0x22: r.append("%22"); break; // "
      case 0x3c: r.append("%3C"); break; // <
      case 0x3e: r.append("%3E"); break; // >
      case 0x5c: r.append("%5C"); break; // backslash
      case 0x5e: r.append("%5E"); break; // ^
      case 0x60: r.append("%60"); break; // `
      case 0x7b: r.append("%7B"); break; // {
      case 0x7c: r.append("%7C"); break; // |
      case 0x7d: r.append("%7D"); break; // }
      case 0x7e: r.append("%7E"); break; // ~

      // Otherwise, append the literal character
      default: r.push_back(s[ii]); break;
    }
  }
  return r;
}


std::string Utils::xml_escape(const std::string& s)
{
  std::string r;
  r.reserve(2*s.length());  // Reserve enough space to avoid continually reallocating.

  for (size_t ii = 0; ii < s.length(); ++ii)
  {
    switch (s[ii])
    {
      case '&':  r.append("&amp;"); break;
      case '\"': r.append("&quot;"); break;
      case '\'': r.append("&apos;"); break;
      case '<':  r.append("&lt;"); break;
      case '>':  r.append("&gt;"); break;

      // Otherwise, append the literal character
      default: r.push_back(s[ii]); break;
    }
  }
  return r;
}


// LCOV_EXCL_START - This function is tested in Homestead's realmmanager_test.cpp
std::string Utils::ip_addr_to_arpa(IP46Address ip_addr)
{
  std::string hostname;

  // Convert the in_addr/in6_addr structure to a string representation
  // of the address. For IPv4 addresses, this is all we need to do.
  // IPv6 addresses contains colons which are not valid characters in
  // hostnames. Instead, we convert the IPv6 address into it's unique
  // reverse lookup form. For example, 2001:dc8::1 becomes
  // 1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.c.d.0.1.0.0.2.ip6.arpa.
  if (ip_addr.af == AF_INET)
  {
    char ipv4_addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_addr.addr.ipv6, ipv4_addr, INET_ADDRSTRLEN);
    hostname = ipv4_addr;
  }
  else if (ip_addr.af == AF_INET6)
  {
    // The output is 32 nibbles of address with separating dots terminated
    // with the string "ip6.arpa", and so will easily be contained
    // in a buffer of size 100.
    char buf[100];
    char* p = buf;
    for (int ii = 15; ii >= 0; ii--)
    {
      // 100 is just the size of the buffer we created earlier.
      p += snprintf(p,
                    100 - (p - buf),
                    "%x.%x.",
                    ip_addr.addr.ipv6.s6_addr[ii] & 0xF,
                    ip_addr.addr.ipv6.s6_addr[ii] >> 4);
    }
    hostname = std::string(buf, p - buf);
    hostname += "ip6.arpa";
  }

  return hostname;
}

/// Generate a random token that only contains valid base64
/// charaters (but doesn't necessarily produce a valid
/// base 64 string as it doesn't check the length and
/// do any padding.
void Utils::create_random_token(size_t length,       //< Number of characters.
                                std::string& token)  //< Destination. Must be empty.
{
  token.reserve(length);

  for (size_t ii = 0; ii < length; ++ii)
  {
    token += _b64[rand() % 64];
  }
}

// This function is from RFC 2617
void Utils::hashToHex(unsigned char *hash_char, unsigned char *hex_char)
{
  unsigned short ii;
  unsigned char jj;
  unsigned char *hc = (unsigned char *) hash_char;

  for (ii = 0; ii < MD5_HASH_SIZE; ii++)
  {
    jj = (hc[ii] >> 4) & 0xf;

    if (jj <= 9)
    {
      hex_char[ii * 2] = (jj + '0');
    }
    else
    {
      hex_char[ii * 2] = (jj + 'a' - 10);
    }

    jj = hc[ii] & 0xf;

    if (jj <= 9)
    {
      hex_char[ii * 2 + 1] = (jj + '0');
    }
    else
    {
      hex_char[ii * 2 + 1] = (jj + 'a' - 10);
    }
  };

  hex_char[HEX_HASH_SIZE] = '\0';
}
// LCOV_EXCL_STOP

bool Utils::StopWatch::_already_logged = false;
