/**
 * @file utils.h Utility functions.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef UTILS_H_
#define UTILS_H_

#include <time.h>
#include <stdint.h>
#include <math.h>
#include <algorithm>
#include <functional>
#include <string>
#include <sstream>
#include <list>
#include <vector>
#include <cctype>
#include <string.h>
#include <sstream>
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
      // Note that we are not simply returning (addr.ipv4.s_addr - rhs.addr.ipv4.s_addr) here.
      // This is deliberate - the "s_addr" field in an in_addr.ipv4 structure is
      // "unsigned long", which means that the difference between two of these
      // might be larger than the maximum value of an int (2Gig - 1). If we
      // were to return (addr.ipv4.s_addr - rhs.addr.ipv4.s_addr) here, we could
      // end up with the compare function saying that A < B < C < A, which ruins
      // the internal structure of C++ maps relying on this comparison function and
      // ultimately causes nasty scribblers.
      //
      // Hence the more careful explicit comparison below.
      if (addr.ipv4.s_addr < rhs.addr.ipv4.s_addr)
      {
        return -1;
      }
      else if (addr.ipv4.s_addr > rhs.addr.ipv4.s_addr)
      {
        return 1;
      }
      else
      {
        return 0;
      }
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

  /// Render the address as a string.
  ///
  /// Note that inet_ntop (which this function uses under the covers) can
  /// technically fail. In this situation this function returns the string
  /// "unknown" (as it is inconvenient if it were allowed to fail).
  std::string to_string()
  {
    char buf[INET6_ADDRSTRLEN];

    if (inet_ntop(af, &addr, buf, sizeof(buf)) != NULL)
    {
      return buf;
    }
    else
    {
      return "unknown";
    }
  }
};

struct AddrInfo
{
  IP46Address address;
  int port;
  int transport;
  int priority;
  int weight;

  AddrInfo():
    priority(1),
    weight(1) {};

  bool operator<(const AddrInfo& rhs) const
  {
    int addr_cmp = address.compare(rhs.address);

    if (addr_cmp < 0)
    {
      return true;
    }
    else if (addr_cmp > 0)
    {
      return false;
    }
    else
    {
      return (port < rhs.port) || ((port == rhs.port) && (transport < rhs.transport));
    }
  }

  bool operator==(const AddrInfo& rhs) const
  {
    return (address.compare(rhs.address) == 0) &&
      (port == rhs.port) &&
      (transport == rhs.transport);
  }

  bool operator!=(const AddrInfo& rhs) const
  {
    return !(operator==(rhs));
  }

  std::string address_and_port_to_string() const
  {
    std::stringstream oss;
    char buf[100];
    if (address.af == AF_INET6)
    {
      oss << "[";
    }
    oss << inet_ntop(address.af, &address.addr, buf, sizeof(buf));
    if (address.af == AF_INET6)
    {
      oss << "]";
    }

    oss << ":" << port;
    return oss.str();
  }

  std::string to_string() const
  {
    std::stringstream oss;
    oss << address_and_port_to_string() << ";transport=";
    if (transport == IPPROTO_SCTP)
    {
      oss << "SCTP";
    }
    else if (transport == IPPROTO_TCP)
    {
      oss << "TCP";
    }
    else if (transport == IPPROTO_UDP)
    {
      oss << "UDP";
    }
    else
    {
      oss << "Unknown (" << transport << ")";
    }

    return oss.str();
  }
};

/// Overrides Google Test default print method to avoid compatibility issues
/// with valgrind
void PrintTo(const AddrInfo& ai, std::ostream* os);

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

  // Converts binary data to an ASCII hex-encoded form, e.g. "\x19\xaf"
  // becomes"19af"
  std::string hex(const uint8_t* data, size_t len);

  /// Splits a URL of the form "http[s]://<servername>[/<path>]" into
  /// servername and path.
  ///
  /// Returns true iff the URL is in the corrects form, and sets the server
  /// and path arguments to the URL components.  If the path in the URL is
  /// missing, it defaults to "/".
  bool parse_http_url(
      const std::string& url,
      std::string& scheme,
      std::string& server,
      std::string& path);

  std::string url_unescape(const std::string& s);
  std::string quote_string(const std::string& s);
  std::string url_escape(const std::string& s);

  std::string xml_escape(const std::string& s);
  inline std::string xml_check_escape(const std::string& s)
  {
    // XML escaping is inefficient. Only do it if the string contains any
    // characters that require escaping.
    if (s.find_first_of("&\"'<>") != std::string::npos)
    {
      return xml_escape(s);
    }
    else
    {
      return s;
    }
  }

  std::string strip_uri_scheme(const std::string& uri);
  std::string remove_visual_separators(const std::string& number);
  bool is_user_numeric(const std::string& user);
  bool is_user_numeric(const char* user, size_t user_len);

  std::string ip_addr_to_arpa(IP46Address ip_addr);

  void create_random_token(size_t length, std::string& token);

  // trim from start
  inline std::string& ltrim(std::string& s)
  {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                    std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
  }

  // trim from end
  inline std::string& rtrim(std::string& s)
  {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
  }

  // trim from both ends
  inline std::string& trim(std::string& s)
  {
    return ltrim(rtrim(s));
  }

  // Strip all whitespace from the string (using the same pattern as the trim
  // functions above - note that this modifies the passed in string)
  inline std::string& strip_whitespace(std::string& s)
  {
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
    return s;
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
  /// at the end of tokens.
  template <class T>  //< container that has T::push_back(std::string)
  void split_string(const std::string& str_in,  //< string to scan (will not be changed)
                    char delimiter,  //< delimiter to use
                    T& tokens,  //< tokens will be added to this list
                    const int max_tokens = 0,  //< max number of tokens to push; last token will be tail of string (delimiters will not be parsed in this section)
                    bool trim = false,  //< trim the string at both ends before splitting?
                    bool check_for_quotes = false,
                    bool include_empty_tokens = false) //< whether empty tokens are counted
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
      if ((token.length() > 0) || (include_empty_tokens))
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
    if ((token.length() > 0) || (include_empty_tokens))
    {
      tokens.push_back(token);
    }
  }

  // Splits a host/port string of the form <IPaddress>:<port> into the address
  // and port.  Supports hostnames (localhost:port), IPv4 addresses
  // (z.y.x.w:port) and IPv6 addresses ([abcd::1234]:port).
  //
  // Returns true on success.
  bool split_host_port(const std::string& host_port,
                       std::string& host,
                       int& port);

  /// Utility function to parse a target name to see if it is a valid IPv4 or IPv6 address.
  bool parse_ip_target(const std::string& target, IP46Address& address);

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
        TRC_ERROR("Failed to get start timestamp: %s", strerror(errno));
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
                      (now.tv_sec - _start.tv_sec) * 1000000L +
                      _elapsed_us;
        }
        else
        {
          TRC_ERROR("Failed to get stop timestamp: %s", strerror(errno));
        }
      }
      else
      {
        result_us = _elapsed_us;
      }

      return _ok;
    }

    /// Adds the specified time in microseconds to the elapsed time
    inline void add_time(unsigned long add_us)
    {
      _elapsed_us += add_us;
    }

    /// Subtracts the specified time in microseconds from the elapsed time
    inline void subtract_time(unsigned long subtract_us)
    {
      _elapsed_us -= subtract_us;
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

  // Compares two 32 bit numbers and returns whether a < b.
  // This also returns true if b has overflowed, and hence looks like b < a
  bool overflow_less_than(uint32_t a, uint32_t b);

  // Compares two 64 bit numbers and returns whether a < b.
  // This also returns true if b has overflowed, and hence looks like b < a
  bool overflow_less_than(uint64_t a, uint64_t b);

  int lock_and_write_pidfile(std::string filename);

  bool parse_stores_arg(const std::vector<std::string>& stores_arg,
                        const std::string& local_site_name,
                        std::string& local_store_location,
                        std::vector<std::string>& remote_stores_locations);

  bool parse_multi_site_stores_arg(const std::vector<std::string>& stores_arg,
                                   const std::string& local_site_name,
                                   const char* store_name,
                                   std::string& store_location,
                                   std::vector<std::string>& remote_stores_locations);

  bool split_site_store(const std::string& site_store,
                        std::string& site,
                        std::string& store);

  // Gets the current time in milliseconds - just a conversion of clock_gettime
  uint64_t get_time(clockid_t clock = CLOCK_MONOTONIC);

  // Daemonize the current process, detaching it from the parent and redirecting
  // file descriptors, with stdout and stderr going to /dev/null.
  int daemonize();

  // Daemonize the current process, detaching it from the parent and redirecting
  // file descriptors.
  int daemonize(std::string out, std::string err);

  // Perform common server setup, daemonizing, and setting up basic logging.
  void daemon_log_setup(int argc,
                        char* argv[],
                        bool daemon,
                        std::string& log_directory,
                        int log_level,
                        bool log_to_file);

  enum IPAddressType {
    IPV4_ADDRESS,
    IPV4_ADDRESS_WITH_PORT,
    IPV6_ADDRESS,
    IPV6_ADDRESS_BRACKETED,
    IPV6_ADDRESS_WITH_PORT,
    INVALID,
    INVALID_WITH_PORT
  };

  // Takes a string and reports what type of IP address it is
  IPAddressType parse_ip_address(std::string address);

  // Takes an IP address and returns it suitable for use in a URI,
  // i.e, an IPv6 address will be returned in brackets.
  // If the optional port is passed, this will be appended if the address
  // doesn't include a port.
  std::string uri_address(std::string address, int default_port = 0);

  // Removes the brackets from an IPv6 address - e.g. [::1] -> ::1
  std::string remove_brackets_from_ip(std::string address);

  // Does the passed in address have brackets?
  bool is_bracketed_address(const std::string& address);

  // Calculates a diameter timeout from the target latency.
  void calculate_diameter_timeout(int target_latency_us,
                                  int& diameter_timeout);

  bool in_vector(const std::string& element,
                 const std::vector<std::string>& elements);

  /// This hook allows a thread to perform actions when Clearwater code does
  /// blocking I/O.
  ///
  /// The caller passes two callbacks when constructing this class: one that is
  /// called when some blocking I/O starts on this thread, and one that is
  /// called when it completes.  Code can signal that I/O is starting/completing
  /// using the helper macros below.
  ///
  /// When an instance of this class is constructed, it is added to the top of a
  /// thread-local stack of IO hooks (implemented as a singly linked list). When
  /// I/O starts or ends, each hook is invoked, starting at the top of the stack
  /// and moving down.
  ///
  /// The destructor removes the hook from the stack. For this reason, it is
  /// important that instances are destroyed in the opposite order they are
  /// created in. This means that hooks should only be stored on the stack and
  /// not on the heap.
  class IOHook
  {
  public:
    /// Type aliases for the callbacks. Each callback takes a string which
    /// contains the reason that the I/O has been performed.
    using IOStartedCallback = std::function<void(const std::string& reason)>;
    using IOCompletedCallback = std::function<void(const std::string& reason)>;

    /// Constructor.
    ///
    /// @param start_cb -    Callback called when the I/O operation starts.
    /// @param complete_cb - Callback called when the I/O operation completes.
    IOHook(IOStartedCallback start_cb,
           IOCompletedCallback complete_cb);

    /// Destructor.
    virtual ~IOHook();

    /// Called to signal the start of an I/O operation.
    /// @param reason - The reason for the I/O operation.
    static void io_starts(const std::string& reason);

    /// Called to signal the completion of an I/O operation.
    /// @param reason - The reason for the I/O operation.
    static void io_completes(const std::string& reason);

    /// No-op implementations of the two callbacks. This is useful for users of
    /// this class that don't want to do anything on one or other of the
    /// callbacks.
    static void NOOP_ON_START(const std::string& reason) {};
    static void NOOP_ON_COMPLETE(const std::string& reason) {};

  private:
    static thread_local std::vector<IOHook*> _hooks;

    IOStartedCallback _io_started_cb;
    IOCompletedCallback _io_completed_cb;
  };
} // namespace Utils

/// Helper macros to make it easier to invoke an I/O hook, and that means the
/// caller does not need to duplicate the reason string.
///
/// Example:
///
///     CW_IO_STARTS("HTTP request")
///     {
///       // Do some blocking I/O.
///     }
///     CW_IO_COMPLETES()
#define CW_IO_STARTS(REASON)                                                   \
  {                                                                            \
    std::string description = REASON;                                          \
    Utils::IOHook::io_starts(description);

#define CW_IO_COMPLETES()                                                      \
    Utils::IOHook::io_completes(description);                                  \
  }

#endif /* UTILS_H_ */
