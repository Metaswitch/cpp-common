/**
 * @file baseresolver.h  Declaration of base class for DNS resolution.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef BASERESOLVER_H__
#define BASERESOLVER_H__

#include <list>
#include <map>
#include <vector>
#include <sstream>
#include <boost/regex.hpp>
#include <pthread.h>

#include "log.h"
#include "dnscachedresolver.h"
#include "ttlcache.h"
#include "utils.h"
#include "sas.h"
#include "weightedselector.h"

class BaseAddrIterator;

/// The BaseResolver class provides common infrastructure for doing DNS
/// resolution, but does not implement a full resolver for any particular
/// protocol.  Specific protocol resolvers are expected to inherit from
/// this class and implement their specific resolution logic using this
/// infrastructure.
class BaseResolver
{
public:
  BaseResolver(DnsCachedResolver* dns_client);
  virtual ~BaseResolver();

  virtual void blacklist(const AddrInfo& ai)
  {
    blacklist(ai, _default_blacklist_duration, _default_graylist_duration);
  }
  virtual void blacklist(const AddrInfo& ai, int blacklist_ttl)
  {
    blacklist(ai, blacklist_ttl, _default_graylist_duration);
  }
  virtual void blacklist(const AddrInfo& ai, int blacklist_ttl, int graylist_ttl);

  /// Indicates that the given AddrInfo has responded.
  virtual void success(const AddrInfo& ai);

  /// Utility function to parse a target name to see if it is a valid IPv4 or IPv6 address.
  static bool parse_ip_target(const std::string& target, IP46Address& address);

  void clear_blacklist();

  // LazyAddrIterator must access the private host_state method of BaseResolver, which
  // it is desirable not to expose
  friend class LazyAddrIterator;

  // Constants indicating the allowed host state values.
  static const int WHITELISTED = 0x01;
  static const int BLACKLISTED = 0x02;
  static const int ALL_LISTS   = WHITELISTED | BLACKLISTED;

protected:
  void create_naptr_cache(std::map<std::string, int> naptr_services);
  void create_srv_cache();
  void create_blacklist(int blacklist_duration)
  {
    // Defaults to not using graylisting
    create_blacklist(blacklist_duration, 0);
  }
  void create_blacklist(int blacklist_duration, int graylist_duration);
  void destroy_naptr_cache();
  void destroy_srv_cache();
  void destroy_blacklist();

  /// Does an SRV record resolution for the specified SRV name, selecting
  /// appropriate targets.
  void srv_resolve(const std::string& srv_name,
                   int af,
                   int transport,
                   int retries,
                   std::vector<AddrInfo>& targets,
                   int& ttl,
                   SAS::TrailId trail,
                   int allowed_host_state=ALL_LISTS);

  /// Does an SRV record resolution for the specified SRV name, selecting
  /// appropriate targets. Returns an iterator pointing to the first target
  BaseAddrIterator* srv_resolve_iter(const std::string& srv_name,
                                     int af,
                                     int transport,
                                     int retries,
                                     int& ttl,
                                     SAS::TrailId trail,
                                     int allowed_host_state=ALL_LISTS);

  /// Does an A/AAAA record resolution for the specified name, selecting
  /// appropriate targets.
  void a_resolve(const std::string& hostname,
                 int af,
                 int port,
                 int transport,
                 int retries,
                 std::vector<AddrInfo>& targets,
                 int& ttl,
                 SAS::TrailId trail,
                 int allowed_host_state=ALL_LISTS);

  /// Does an A/AAAA record resolution for the specified name, and returns an
  /// Iterator that lazily selects appropriate targets.
  virtual BaseAddrIterator* a_resolve_iter(const std::string& hostname,
                                           int af,
                                           int port,
                                           int transport,
                                           int& ttl,
                                           SAS::TrailId trail,
                                           int allowed_host_state);

  /// Converts a DNS A or AAAA record to an IP46Address structure.
  IP46Address to_ip46(const DnsRRecord* rr);

  /// Holds the results of applying NAPTR replacement on a target domain name.
  struct NAPTRReplacement
  {
    std::string replacement;
    std::string flags;
    int transport;
  };

  /// Factory class to handle populating and evicting entries from the
  /// NAPTR cache.
  class NAPTRCacheFactory : public CacheFactory<std::string, NAPTRReplacement*>
  {
  public:
    NAPTRCacheFactory(const std::map<std::string, int>& services,
                      int default_ttl,
                      DnsCachedResolver* dns_client);
    virtual ~NAPTRCacheFactory();

    NAPTRReplacement* get(std::string key, int& ttl, SAS::TrailId trail);
    void evict(std::string key, NAPTRReplacement* value);

  private:
    static bool compare_naptr_order_preference(DnsNaptrRecord* r1,
                                               DnsNaptrRecord* r2);
    bool parse_regex_replace(const std::string& regex_replace,
                             boost::regex& regex,
                             std::string& replace);

    std::map<std::string, int> _services;
    int _default_ttl;
    DnsCachedResolver* _dns_client;
  };
  NAPTRCacheFactory* _naptr_factory;

  /// The NAPTR cache holds a cache of the results of performing a NAPTR
  /// lookup on a particular target.
  typedef TTLCache<std::string, NAPTRReplacement*> NAPTRCache;
  NAPTRCache* _naptr_cache;

  /// The SRVPriorityList holds the result of an SRV lookup sorted into
  /// priority groups.
  struct SRV
  {
    std::string target;
    int port;
    int priority;
    int weight;
    int get_weight() const
    {
      return weight;
    }
  };
  typedef std::map<int, std::vector<SRV> > SRVPriorityList;

  /// Factory class to handle populating and evicting entries from the SRV
  /// cache.
  class SRVCacheFactory : public CacheFactory<std::string, SRVPriorityList*>
  {
  public:
    SRVCacheFactory(int default_ttl, DnsCachedResolver* dns_client);
    virtual ~SRVCacheFactory();

    SRVPriorityList* get(std::string key, int&ttl, SAS::TrailId trail);
    void evict(std::string key, SRVPriorityList* value);

  private:
    static bool compare_srv_priority(DnsRRecord* r1, DnsRRecord* r2);

    int _default_ttl;
    DnsCachedResolver* _dns_client;
  };
  SRVCacheFactory* _srv_factory;

  /// The SRV cache holds a cache of SRVPriorityLists indexed on the SRV domain
  /// name (that is, a domain of the form _<service>._<transport>.<target>).
  typedef TTLCache<std::string, SRVPriorityList*> SRVCache;
  SRVCache* _srv_cache;

  /// The global hosts map holds a list of IP/transport/port combinations which
  /// have been blacklisted because the destination is unresponsive (either TCP
  /// connection attempts are failing or a UDP destination is unreachable).
  ///
  /// Blacklisted hosts are not given out by the a_resolve method, unless
  /// insufficient non-blacklisted hosts are available. A host remains on the
  /// blacklist until a specified time has elapsed, after which it moves to the
  /// graylist.
  ///
  /// Hosts on the graylist are given out by the a_resolve method to only one
  /// client, unless insufficient non-blacklisted hosts are available. A host
  /// moves to the whitelist if the client probing this host connects
  /// successfully, or if a specified time elapses. If a client given a host for
  /// probing and does not attempt to connect to it, it is made available for
  /// giving out by a_resolve once more. If a client attempts but fails to
  /// connect to a host, it is moved to the blacklist.

  /// Private class to hold data and methods associated to an IP/transport/port
  /// combination in the blacklist system. Each Host is associated with exactly
  /// one such combination.
  class Host
  {
  public:
    /// Constructor
    /// @param blacklist_ttl The time in seconds for the host to remain on the
    ///                      blacklist before moving to the graylist
    /// @param graylist_ttl  The time in seconds for the host to remain on the
    ///                      graylist before moving to the whitelist
    Host(int blacklist_ttl, int graylist_ttl);

    /// Destructor
    ~Host();

    /// Enum representing the current state of a Host in the blacklist
    /// system. Whitelisted, graylisted and not selected for probing, graylisted
    /// and selected for probing, and blacklisted respectively.
    enum struct State {WHITE, GRAY_NOT_PROBING, GRAY_PROBING, BLACK};

    /// Returns a string representation of the given state.
    static std::string state_to_string(State state);

    /// Returns the state of this Host at the given time in seconds since the
    /// epoch
    State get_state() {return get_state(time(NULL));}
    State get_state(time_t current_time);

    /// Indicates that this Host has been successfully contacted.
    void success();

    /// Indicates that this Host is selected for probing by the given user.
    void selected_for_probing(pthread_t user_id);

  private:
    /// The time in seconds since the epoch at which this Host is to be removed
    /// from the blacklist and placed onto the graylist.
    time_t _blacklist_expiry_time;

    /// The time in seconds since the epoch at which this Host is to be removed
    /// from the graylist.
    time_t _graylist_expiry_time;

    /// Indicates that this Host is currently being probed.
    bool _being_probed;

    /// The ID of the thread currently probing this Host.
    pthread_t _probing_user_id;
  };

  pthread_mutex_t _hosts_lock;
  typedef std::map<AddrInfo, Host> Hosts;
  Hosts _hosts;

  /// Returns the state of the Host associated with the given AddrInfo, if it is
  /// in the blacklist system, and Host::State::WHITE otherwise. _hosts_lock
  /// must be held when calling this method.
  Host::State host_state(const AddrInfo& ai) {return host_state(ai, time(NULL));}
  Host::State host_state(const AddrInfo& ai, time_t current_time);

  /// Returns true if the state of the host associated with the given AddrInfo
  /// is black or either type of gray, since those are treated as blacklisted.
  /// Note that even if the address is graylisted and currently being probed by the request
  /// calling this function, the address will still be considered blacklisted
  bool blacklisted(const AddrInfo& ai);

  /// Indicates that the calling thread is selected to probe the given AddrInfo.
  /// _hosts_lock must be held when calling this method.
  void select_for_probing(const AddrInfo& ai);

  int _default_blacklist_duration;
  int _default_graylist_duration;

  /// Stores a pointer to the DNS client this resolver should use.
  DnsCachedResolver* _dns_client;

  static const int DEFAULT_TTL = 300;

};

// Abstract base class for AddrInfo iterators used in target selection.
class BaseAddrIterator
{
public:
  BaseAddrIterator() {}
  virtual ~BaseAddrIterator() {}

  /// Should return a vector containing at most num_requested_targets AddrInfo
  /// targets.
  virtual std::vector<AddrInfo> take(int num_requested_targets) = 0;

  /// If any unused targets remain, sets the value of target to the next one and
  /// returns true. Otherwise returns false and leaves the value of target
  /// unchanged.
  virtual bool next(AddrInfo &target);
};

// AddrInfo iterator that simply returns the targets it is given in sequence.
class SimpleAddrIterator : public BaseAddrIterator
{
public:
  SimpleAddrIterator(std::vector<AddrInfo> targets) : _targets(targets) {}
  virtual ~SimpleAddrIterator() {}

  /// Returns a vector containing the first num_requested_targets elements of
  /// _targets, or all the elements of _targets if num_requested_targets is
  /// greater than the size of _targets.
  virtual std::vector<AddrInfo> take(int num_requested_targets);

private:
  std::vector<AddrInfo> _targets;
};

// AddrInfo iterator that uses the blacklist system of a BaseResolver to lazily
// select targets.
class LazyAddrIterator : public BaseAddrIterator
{
public:
  // Constructor. The take method requires that resolver is not NULL.
  LazyAddrIterator(DnsResult& dns_result,
                   BaseResolver* resolver,
                   int port,
                   int transport,
                   SAS::TrailId trail,
                   int allowed_host_state);
  virtual ~LazyAddrIterator() {}

  /// Returns a vector containing at most num_requested_targets AddrInfo targets,
  /// selected based on their current state in the blacklist system of
  /// resolver.
  virtual std::vector<AddrInfo> take(int num_requested_targets);

private:
  // A vector that initially contains the results of a DNS query. As results
  // are returned from the take method, or moved to the vector of unhealthy
  // results, they are removed from this vector
  std::vector<AddrInfo> _unused_results;

  // Used to store DNS results corresponding to unhealthy hosts
  std::vector<AddrInfo> _unhealthy_results;

  // A pointer to the BaseResolver that created this iterator
  BaseResolver* _resolver;

  // The allowed state of hosts returned by this iterator
  int _allowed_host_state;

  std::string _hostname;
  SAS::TrailId _trail;

  // True if the iterator has not yet been called, and false otherwise
  bool _first_call;
};
#endif
