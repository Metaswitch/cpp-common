/**
 * @file utils.cpp Utility functions.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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

#include <stdio.h>
#include <unistd.h>
#include <sys/file.h>
#include <signal.h>
#include <sys/stat.h>
#include <syslog.h>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include "utils.h"

bool Utils::parse_http_url(
    const std::string& url,
    std::string& scheme,
    std::string& server,
    std::string& path)
{
  size_t colon_pos = url.find(':');
  if (colon_pos == std::string::npos)
  {
    // No colon - no good!
    return false;
  }

  scheme = url.substr(0, colon_pos);
  if ((scheme != "http") && (scheme != "https"))
  {
    // Not HTTP or HTTPS.
    return false;
  }
  size_t slash_slash_pos = url.find("//", colon_pos + 1);
  if (slash_slash_pos != colon_pos + 1)
  {
    // Not full URL.
    return false;
  }
  size_t slash_pos = url.find('/', slash_slash_pos + 2);
  if (slash_pos == std::string::npos)
  {
    // No path.
    server = url.substr(slash_slash_pos + 2);
    path = "/";
  }
  else
  {
    server = url.substr(slash_slash_pos + 2, slash_pos - (slash_slash_pos + 2));
    path = url.substr(slash_pos);
  }
  return true;
}

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

// The following function quotes strings in SIP headers as described by RFC 3261
// Section 25.1
std::string Utils::quote_string(const std::string& s)
{
  std::string r = "\"";
  r.reserve((2*s.length()) + 2); // Reserve enough space to avoid continually reallocating.

  for (size_t ii = 0; ii < s.length(); ++ii)
  {
    char unquot = s[ii];
    switch (unquot)
    {
      case '"':
      case '\\':
        r.push_back('\\');
        break;

      default:
        break;
    }

    r.push_back(unquot);
  }

  r.push_back('"');

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

std::string Utils::strip_uri_scheme(const std::string& uri)
{
  std::string s(uri);
  size_t colon = s.find(':');

  if (colon != std::string::npos)
  {
    s.erase(0, colon + 1);
  }

  return s;
}

std::string Utils::remove_visual_separators(const std::string& number)
{
  static const boost::regex CHARS_TO_STRIP = boost::regex("[.)(-]");
  return boost::regex_replace(number, CHARS_TO_STRIP, std::string(""));
}

bool Utils::is_user_numeric(const std::string& user)
{
  return is_user_numeric(user.c_str(), user.length());
}

bool Utils::is_user_numeric(const char* user, size_t user_len)
{
  for (size_t i = 0; i < user_len; i++)
  {
    if (((user[i] >= '0') &&
         (user[i] <= '9')) ||
        (user[i] == '+') ||
        (user[i] == '-') ||
        (user[i] == '.') ||
        (user[i] == '(') ||
        (user[i] == ')') ||
        (user[i] == '[') ||
        (user[i] == ']'))
    {
      continue;
    }
    else
    {
      return false;
    }
  }

  return true;
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

std::string Utils::hex(const uint8_t* data, size_t len)
{
  static const char* const hex_lookup = "0123456789abcdef";
  std::string result;
  result.reserve(2 * len);
  for (size_t ii = 0; ii < len; ++ii)
  {
    const uint8_t b = data[ii];
    result.push_back(hex_lookup[b >> 4]);
    result.push_back(hex_lookup[b & 0x0f]);
  }
  return result;
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

bool Utils::split_host_port(const std::string& host_port,
                            std::string& host,
                            int& port)
{
  // The address is specified as either <hostname>:<port>, <IPv4 address>:<port>
  // or [<IPv6 address>]:<port>.  Look for square brackets to determine whether
  // this is an IPv6 address.
  std::vector<std::string> host_port_parts;
  size_t close_bracket = host_port.find(']');

  if (close_bracket == host_port.npos)
  {
    // IPv4 connection.  Split the string on the colon.
    Utils::split_string(host_port, ':', host_port_parts, 0, false, false, true);
    if (host_port_parts.size() != 2)
    {
      TRC_DEBUG("Malformed host/port %s", host_port.c_str());
      return false;
    }
  }
  else
  {
    // IPv6 connection.  Split the string on ']', which removes any white
    // space from the start and the end, then remove the '[' from the
    // start of the IP address string and the start of the ':' from the start
    // of the port string.
    Utils::split_string(host_port, ']', host_port_parts);
    if ((host_port_parts.size() != 2) ||
        (host_port_parts[0][0] != '[') ||
        (host_port_parts[1][0] != ':'))
    {
      TRC_DEBUG("Malformed host/port %s", host_port.c_str());
      return false;
    }

    host_port_parts[0].erase(host_port_parts[0].begin());
    host_port_parts[1].erase(host_port_parts[1].begin());
  }

  port = atoi(host_port_parts[1].c_str());
  host = host_port_parts[0];

  // Check the port was parsed correctly.
  if (std::to_string(port) != host_port_parts[1])
  {
    TRC_DEBUG("Malformed port %s", host_port_parts[1].c_str());
    return false;
  }

  return true;
}

/// Parses a target as if it was an IPv4 or IPv6 address and returns the
/// status of the parse.
bool Utils::parse_ip_target(const std::string& target, IP46Address& address)
{
  // Assume the parse fails.
  TRC_DEBUG("Attempt to parse %s as IP address", target.c_str());
  bool rc = false;

  // Strip start and end white-space, and any brackets if this is an IPv6
  // address
  std::string ip_target = Utils::remove_brackets_from_ip(target);
  Utils::trim(ip_target);

  if (inet_pton(AF_INET6, ip_target.c_str(), &address.addr.ipv6) == 1)
  {
    // Parsed the address as a valid IPv6 address.
    address.af = AF_INET6;
    rc = true;
  }
  else if (inet_pton(AF_INET, ip_target.c_str(), &address.addr.ipv4) == 1)
  {
    // Parsed the address as a valid IPv4 address.
    address.af = AF_INET;
    rc = true;
  }

  return rc;
}

bool Utils::overflow_less_than(uint32_t a, uint32_t b)
{
    return ((a - b) > ((uint32_t)(1) << 31));
}

bool Utils::overflow_less_than(uint64_t a, uint64_t b)
{
    return ((a - b) > ((uint64_t)(1) << 63));
}

int Utils::lock_and_write_pidfile(std::string filename)
{
  std::string lockfilename = filename + ".lock";
  int lockfd = open(lockfilename.c_str(),
                    O_WRONLY | O_CREAT,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  int rc = flock(lockfd, LOCK_EX | LOCK_NB);
  if (rc == -1)
  {
    close(lockfd);
    return -1;
  }

  FILE* fd = fopen(filename.c_str(), "w");
  fprintf(fd, "%d\n", getpid());
  fclose(fd);

  return lockfd;
}

/// Parse a vector of strings of the form <site>=<store>. Use the name of the
/// locate GR site to produce the location of the local site's store, and a
/// vector of the locations of the remote sites' stores. If only one store is
/// provided, it may not be identified by a site - we just assume it's the
/// local_site.
///
/// @param stores_arg        - the input vector.
/// @param local_site_name   - the input local site name.
/// @local_store_location    - the output local store location.
/// @remote_stores_locations - the output vector of remote store locations.
/// @returns                 - true if the stores_arg vector contains a set of
///                            valid values, false otherwise.
bool Utils::parse_stores_arg(const std::vector<std::string>& stores_arg,
                             const std::string& local_site_name,
                             std::string& local_store_location,
                             std::vector<std::string>& remote_stores_locations)
{
  if ((stores_arg.size() == 1) &&
      (stores_arg.front().find("=") == std::string::npos))
  {
    // If only one store is provided then it may not be identified by a site -
    // we just assume that it is the store in the single local site.
    local_store_location = stores_arg.front();
  }
  else
  {
    // If multiple stores are provided and they should all be identified by a
    // site. Save the local site's store off separately to the remote sites'
    // stores.
    for (std::vector<std::string>::const_iterator it = stores_arg.begin();
         it != stores_arg.end();
         ++it)
    {
      std::string site;
      std::string store;
      if (!split_site_store(*it, site, store))
      {
        // Expected <site_name>=<domain>.
        return false;
      }
      else if (site == local_site_name)
      {
        // This is the local site's store.
        local_store_location = store;
      }
      else
      {
        // A remote site store.
        remote_stores_locations.push_back(store);
      }
    }
  }

  return true;
}

/// Split a string of the form <site>=<store> into the site and the store. If no
/// site is specified then assume the whole string is the store, but return
/// false.
///
/// @param site_store - the input string.
/// @param site       - the output site (or the empty string if no site is
///                     specified).
/// @param store      - the output store.
/// @return           - true if the store is identified by a site, false
///                     otherwise.
bool Utils::split_site_store(const std::string& site_store,
                             std::string& site,
                             std::string& store)
{
  size_t pos = site_store.find("=");
  if (pos == std::string::npos)
  {
    // No site specified.
    site = "";
    store = site_store;
    return false;
  }
  else
  {
    // Find the site and the store.
    site = site_store.substr(0, pos);
    store = site_store.substr(pos+1);
    return true;
  }
}

/// Parse a vector of strings of the form <site>=<store>. Use the name of the
/// local GR site to produce the location of the local site's store, and a
/// vector of the locations of the remote sites' stores. If only one store is
/// provided, it may not be identified by a site - we just assume it's the
/// local_site.
///
/// @param stores_arg        - the input vector.
/// @param local_site_name   - the input local site name.
/// @param store_name        - the name of the store (e.g. IMPI store) for
///                            logging
/// @local_store_location    - the output local store location.
/// @remote_stores_locations - the output vector of remote store locations.
/// @returns                 - true if the stores_arg vector contains a set of
///                            valid values, false otherwise.
bool Utils::parse_multi_site_stores_arg(const std::vector<std::string>& stores_arg,
                                        const std::string& local_site_name,
                                        const char* store_name,
                                        std::string& store_location,
                                        std::vector<std::string>& remote_stores_locations)
{
  if (!stores_arg.empty())
  {
    if (!Utils::parse_stores_arg(stores_arg,
                                 local_site_name,
                                 store_location,
                                 remote_stores_locations))
    {
      TRC_ERROR("Invalid format of %s program argument", store_name);
      return false;
    }

    if (store_location == "")
    {
      // If we've failed to find a local store then this is a misconfiguration.
      TRC_ERROR("No local site %s specified", store_name);
      return false;
    }
    else
    {
      TRC_INFO("Using %s", store_name);
      TRC_INFO("  Primary store: %s", store_location.c_str());
      std::string remote_stores_str = boost::algorithm::join(remote_stores_locations, ", ");
      TRC_INFO("  Backup store(s): %s", remote_stores_str.c_str());
    }
  }

  return true;
}

uint64_t Utils::get_time(clockid_t clock)
{
  struct timespec ts;
  clock_gettime(clock, &ts);
  uint64_t timestamp = ((uint64_t)ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
  return timestamp;
}

int Utils::daemonize()
{
  return daemonize("/dev/null", "/dev/null");
}

int Utils::daemonize(std::string out, std::string err)
{
  TRC_STATUS("Switching to daemon mode");

  // First fork
  pid_t pid = fork();
  if (pid == -1)
  {
    return errno;
  }
  else if (pid > 0)
  {
    // Parent process, fork successful, so exit.
    exit(0);
  }

  // Redirect standard files to /dev/null
  if (freopen("/dev/null", "r", stdin) == NULL)
  {
    return errno;
  }
  if (freopen(out.c_str(), "a", stdout) == NULL)
  {
    return errno;
  }
  if (freopen(err.c_str(), "a", stderr) == NULL)
  {
    return errno;
  }

  // Create a new session to divorce the child from the tty of the parent.
  if (setsid() == -1)
  {
    return errno;
  }

  // Clear any restricted umask
  umask(0);

  // Second fork
  pid = fork();
  if (pid == -1)
  {
    return errno;
  }
  else if (pid > 0)
  {
    // Parent process, fork successful, so exit.
    exit(0);
  }

  return 0;
}

void Utils::daemon_log_setup(int argc,
                             char* argv[],
                             bool daemon,
                             std::string& log_directory,
                             int log_level,
                             bool log_to_file)
{
  // Work out the program name from argv[0], stripping anything before the
  // final slash.
  char* prog_name = argv[0];
  char* slash_ptr = rindex(argv[0], '/');
  if (slash_ptr != NULL)
  {
    prog_name = slash_ptr + 1;
  }

  // Copy the program name to a string so that we can be sure of its lifespan -
  // the memory passed to openlog must be valid for the duration of the program.
  //
  // Note that we don't save syslog_identity here, and so we're technically leaking
  // this object. However, its effectively part of static initialisation of
  // the process - it'll be freed on process exit - so it's not leaked in practice.
  std::string* syslog_identity = new std::string(prog_name);

  // Open a connection to syslog. This is used for different purposes - e.g. ENT
  // logs and analytics logs. We use the same facility for all purposes because
  // calling openlog with a different facility each time we send a log to syslog
  // is not trivial to make thread-safe.
  openlog(syslog_identity->c_str(), LOG_PID, LOG_LOCAL7);

  if (daemon)
  {
    int errnum;

    if (log_directory != "")
    {
      std::string prefix = log_directory + "/" + prog_name;
      errnum = Utils::daemonize(prefix + "_out.log",
                                prefix + "_err.log");
    }
    else
    {
      errnum = Utils::daemonize();
    }

    if (errnum != 0)
    {
      TRC_ERROR("Failed to convert to daemon, %d (%s)", errnum, strerror(errnum));
      exit(0);
    }
  }

  Log::setLoggingLevel(log_level);

  if ((log_to_file) && (log_directory != ""))
  {
    Log::setLogger(new Logger(log_directory, prog_name));
  }

  TRC_STATUS("Log level set to %d", log_level);
}

bool Utils::is_bracketed_address(const std::string& address)
{
  return ((address.size() >= 2) &&
          (address[0] == '[') &&
          (address[address.size() - 1] == ']'));
}

std::string Utils::remove_brackets_from_ip(std::string address)
{
  bool bracketed = is_bracketed_address(address);
  return bracketed ? address.substr(1, address.size() - 2) :
                     address;
}

std::string Utils::uri_address(std::string address, int default_port)
{
  Utils::IPAddressType addrtype = parse_ip_address(address);

  if (default_port == 0)
  {
    if (addrtype == IPAddressType::IPV6_ADDRESS)
    {
      address = "[" + address + "]";
    }
  }
  else
  {
    std::string port = std::to_string(default_port);

    if (addrtype == IPAddressType::IPV4_ADDRESS ||
        addrtype == IPAddressType::IPV6_ADDRESS_BRACKETED ||
        addrtype == IPAddressType::INVALID)
    {
      address = address + ":" + port;
    }
    else if (addrtype == IPAddressType::IPV6_ADDRESS)
    {
      address = "[" + address + "]:" + port;
    }
  }

  return address;
}

Utils::IPAddressType Utils::parse_ip_address(std::string address)
{
  // Check if we have a port
  std::string host;
  int port;
  bool with_port = Utils::split_host_port(address, host, port);

  // We only want the host part of the address.
  host = with_port ? host : address;

  // Check if we're surrounded by []
  bool with_brackets = is_bracketed_address(host);

  host = with_brackets ? host.substr(1, host.size() - 2) : host;

  // Check if we're IPv4/IPv6/invalid
  struct in_addr dummy_ipv4_addr;
  struct in6_addr dummy_ipv6_addr;

  if (inet_pton(AF_INET, host.c_str(), &dummy_ipv4_addr) == 1)
  {
    return (with_port) ? IPAddressType::IPV4_ADDRESS_WITH_PORT :
                         IPAddressType::IPV4_ADDRESS;
  }
  else if (inet_pton(AF_INET6, host.c_str(), &dummy_ipv6_addr) == 1)
  {
    return (with_port) ? IPAddressType::IPV6_ADDRESS_WITH_PORT :
                         ((with_brackets) ? IPAddressType::IPV6_ADDRESS_BRACKETED :
                                            IPAddressType::IPV6_ADDRESS);
  }
  else
  {
    return (with_port) ? IPAddressType::INVALID_WITH_PORT :
                         IPAddressType::INVALID;
  }
}

void Utils::calculate_diameter_timeout(int target_latency_us,
                                       int& diameter_timeout_ms)
{
  // Set the diameter timeout to twice the target latency (rounding up). Note
  // that the former is expressed in milliseconds and the latter in
  // microseconds, hence division by 500 (i.e. multiplication by 2/1000).
  diameter_timeout_ms = std::ceil(target_latency_us/500);
}

// Functions to add a var param to a SAS event, compressed or not, depending on whether compression
// is disabled in etc/clearwater/shared_config.
void Utils::add_sas_param_compressed_if_toggled(SAS::Event& event,
                                                const std::string& s,
                                                const SAS::Profile* profile,
                                                bool sas_compress_logs)
{
  if (sas_compress_logs)
  {
    event.add_compressed_param(s, profile);
  }
  else
  {
    event.add_var_param(s);
  }
  event.add_static_param(sas_compress_logs);
}

void Utils::add_sas_param_compressed_if_toggled(SAS::Event& event,
                                                size_t len,
                                                char* s,
                                                const SAS::Profile* profile,
                                                bool sas_compress_logs)
{
  if (sas_compress_logs)
  {
    event.add_compressed_param(len, s, profile);
  }
  else
  {
    event.add_var_param(len, s);
  }
  event.add_static_param(sas_compress_logs);
}

void Utils::add_sas_param_compressed_if_toggled(SAS::Event& event,
                                                size_t len,
                                                uint8_t* s,
                                                const SAS::Profile* profile,
                                                bool sas_compress_logs)
{
  if (sas_compress_logs)
  {
    event.add_compressed_param(len, s, profile);
  }
  else
  {
    event.add_var_param(len, s);
  }
  event.add_static_param(sas_compress_logs);
}

void Utils::add_sas_param_compressed_if_toggled(SAS::Event& event,
                                                const char* s,
                                                const SAS::Profile* profile,
                                                bool sas_compress_logs)
{
  if (sas_compress_logs)
  {
    event.add_compressed_param(s, profile);
  }
  else
  {
    event.add_var_param(s);
  }
  event.add_static_param(sas_compress_logs);
}

// Check whether an element is in a vector
bool Utils::in_vector(const std::string& element,
               const std::vector<std::string>& vec)
{
  return std::find(vec.begin(), vec.end(), element) != vec.end();
}

//
// IOHook methods.
//

Utils::IOHook::IOHook(IOStartedCallback start_cb,
                      IOCompletedCallback complete_cb) :
  _io_started_cb(start_cb),
  _io_completed_cb(complete_cb)
{
  _hooks.push_back(this);
  TRC_DEBUG("Added IOHook %p to stack. There are now %d hooks", this, _hooks.size());
}

Utils::IOHook::~IOHook()
{
  _hooks.erase(std::remove(_hooks.begin(), _hooks.end(), this));
  TRC_DEBUG("Removed IOHook %p to stack. There are now %d hooks", this, _hooks.size());
}

void Utils::IOHook::io_starts(const std::string& reason)
{
  // Iterate through the hooks in reverse order so the one most recently
  // registered gets invoked first.
  for(std::vector<IOHook*>::reverse_iterator hook = _hooks.rbegin();
      hook != _hooks.rend();
      hook++)
  {
    (*hook)->_io_started_cb(reason);
  }
}

void Utils::IOHook::io_completes(const std::string& reason)
{
  // Iterate through the hooks in reverse order so the one most recently
  // registered gets invoked first.
  for(std::vector<IOHook*>::reverse_iterator hook = _hooks.rbegin();
      hook != _hooks.rend();
      hook++)
  {
    (*hook)->_io_completed_cb(reason);
  }
}

thread_local std::vector<Utils::IOHook*> Utils::IOHook::_hooks = {};


