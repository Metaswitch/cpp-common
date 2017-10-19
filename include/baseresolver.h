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

  void clear_blacklist();

  // LazyAResolveIter and LazySRVResolveIter must access the private host_state
  // method of BaseResolver, which it is desirable not to expose
  friend class LazyAResolveIter;
  friend class LazySRVResolveIter;

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

  /// Creates and returns a LazySRVResolveIter. This will lazily return targets
  /// when its take method is called, ie. The iterator finds addresses and
  /// returns a vector of targets. This ensures that the targets returned have
  /// the desired host state, even if this changed after the iterator was
  /// created.
  BaseAddrIterator* srv_resolve_iter(const std::string& srv_name,
                                     int af,
                                     int transport,
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

  /// Called to check whether the base resolver is happy with an address being
  /// used as a target. It is allowed to reject the address if the current state
  /// of the address is incompatible with the allowed host state.
  ///
  /// By calling this method, the caller guarantees that it will use the address
  /// (assuming it is found to be acceptable to the resolver).
  ///
  /// @param addr               - The address to check.
  /// @param trail              - SAS trail ID.
  /// @param allowed_host_state - A bitmask containing the allowed hosts states.
  ///
  /// @return                   - Whether the address is acceptable.
  bool select_address(const AddrInfo& addr,
                      SAS::TrailId trail,
                      int allowed_host_state=ALL_LISTS);

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
  class NAPTRCacheFactory : public CacheFactory<std::string, NAPTRReplacement>
  {
  public:
    NAPTRCacheFactory(const std::map<std::string, int>& services,
                      int default_ttl,
                      DnsCachedResolver* dns_client);
    virtual ~NAPTRCacheFactory();

    std::shared_ptr<NAPTRReplacement> get(std::string key, int& ttl, SAS::TrailId trail);

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
  typedef TTLCache<std::string, NAPTRReplacement> NAPTRCache;
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

  /// Factory class to handle populating entries from the SRV cache.
  class SRVCacheFactory : public CacheFactory<std::string, SRVPriorityList>
  {
  public:
    SRVCacheFactory(int default_ttl, DnsCachedResolver* dns_client);
    virtual ~SRVCacheFactory();

    std::shared_ptr<SRVPriorityList> get(std::string key, int&ttl, SAS::TrailId trail);

  private:
    static bool compare_srv_priority(DnsRRecord* r1, DnsRRecord* r2);

    int _default_ttl;
    DnsCachedResolver* _dns_client;
  };
  SRVCacheFactory* _srv_factory;

  /// The SRV cache holds a cache of SRVPriorityLists indexed on the SRV domain
  /// name (that is, a domain of the form _<service>._<transport>.<target>).
  typedef TTLCache<std::string, SRVPriorityList> SRVCache;
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

  /// Indicates that the calling thread is selected to probe the given AddrInfo.
  /// _hosts_lock must be held when calling this method.
  void select_for_probing(const AddrInfo& ai);

  /// Helper function to create SAS logs if no targets were resolved. Says if
  /// this was because only whitelisted or blacklisted targets were requested,
  /// or if there were no records at all for that address
  void no_targets_resolved_logging(const std::string name,
                                   SAS::TrailId trail,
                                   bool whitelisted_allowed,
                                   bool blacklisted_allowed);

  /// Utility function for building up strings representing targets to log to
  /// SAS.
  ///
  /// @param log_string - Logging string that is updated in place.
  /// @param addr       - The address to log.
  /// @param state      - The target's state for the purposes of SAS logging.
  static void add_target_to_log_string(std::string& log_string,
                                       const AddrInfo& addr,
                                       const std::string& state);

  /// Allows DNS Resolution to be called with a pointer to the Base Resolver.
  /// This just returns the (possibly cached) result of a DNS Query, so any
  /// post-processing of the BaseResolver is not returned.
  void dns_query(std::vector<std::string>& domains,
                 int dnstype,
                 std::vector<DnsResult>& results,
                 SAS::TrailId trail);

  /// Helper function to perform SRV Record DNS Resolution via the SRV Cache and
  /// returns a pointer to an SRV Priority List for the given SRV name.
  std::shared_ptr<SRVPriorityList> get_srv_list(const std::string& srv_name,
                                                int &ttl,
                                                SAS::TrailId trail);

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
// select targets using A Record Resolution.
class LazyAResolveIter : public BaseAddrIterator
{
public:
  // Constructor. The take method requires that resolver is not NULL.
  LazyAResolveIter(DnsResult& dns_result,
                   BaseResolver* resolver,
                   int port,
                   int transport,
                   SAS::TrailId trail,
                   int allowed_host_state);
  virtual ~LazyAResolveIter() {}

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

// AddrInfo iterator that uses the blacklist system of a BaseResolver to lazily
// select targets using SRV Record Resolution.
class LazySRVResolveIter : public BaseAddrIterator
{
public:
  // Constructor. The take method requires that resolver is not nullptr.
  LazySRVResolveIter(BaseResolver* resolver,
                     int af,
                     int transport,
                     const std::string& srv_name,
                     SAS::TrailId trail,
                     int allowed_host_state);
  virtual ~LazySRVResolveIter() {}

  /// Returns a vector containing at most num_requested_targets AddrInfo targets,
  /// selected based on their current state in the blacklist system of
  /// resolver.
  std::vector<AddrInfo> take(int num_requested_targets);

  /// Returns the smallest time to live found for the list of SRVs and various A
  /// Record DNS resolutions so far.
  int get_min_ttl();

private:
  /// Prepares a whole priority level by applying A/AAAA Record Resolution to
  /// find the addresses for each SRV, and storing the results in
  /// _whitelisted_addresses_by_srv and _unhealthy_addresses_by_srv. The order
  /// of the SRVs is random and based on the weights, and the order of addresses
  /// for an SRV is uniformly random.
  ///
  /// If the iterator is still looking for its first target, ie.
  /// _search_for_gray is true, then the entire priority level will be searched
  /// for an unprobed graylisted address. If one is found then _gray_found will
  /// be set to true and it will be stored in _unprobed_gray_target and not in
  /// the *_addresses_by_srv vectors.
  ///
  /// Returns true if the priority level was prepared successfully, false if
  /// there were no priority levels left to prepare
  bool prepare_priority_level();

  /// If both blacklisted and whitelisted addresses were requested goes through
  /// the vectors returned by prepare_priority_level, adding whitelisted
  /// addresses to targets and black and gray addresses to _unhealthy_targets.
  /// If only whitelisted targets were requested nothing is added to
  /// _unhealthy_targets. If only blacklisted targets were requested, it instead
  /// adds black and gray addresses to targets and nothing to
  /// _unhealthy_targets.
  ///
  /// Returns the number of targets that are still to be found, or 0 if
  /// num_targets_to_find were all found. If all targets were found, the search
  /// is paused and the position stored in _current_srv, to be resumed when take
  /// is next called.
  ///
  /// The last argument receives a string containing the list of targets
  /// selected, for SAS logging.
  int get_from_priority_level(std::vector<AddrInfo> &targets,
                              int num_targets_to_find,
                              int num_requested_targets,
                              std::string& targets_log_str);

  /// Helper function that returns true if get_from_priority_level has looked at
  /// every address in both *_addresses_by_srv vectors, or if no priority level
  /// has been prepared yet.
  bool priority_level_complete();

  // A pointer to the BaseResolver that created this iterator. Used to access
  // Base Resolver methods such as host_state.
  BaseResolver* _resolver;

  // The allowed state of hosts returned by this iterator.
  bool _whitelisted_allowed;
  bool _blacklisted_allowed;

  int _af;
  int _transport;
  std::string _srv_name;

  // The smallest time to live found so far, from all DNS Resolutions. Updated
  // by prepare_priority_level if the DNS resolution finds a smaller time to
  // live.
  int _ttl;

  // Pointer to a map from priority levels to a vector of SRVs for that priority
  // level.
  std::shared_ptr<BaseResolver::SRVPriorityList> _srv_list;

  SAS::TrailId _trail;

  // Tells prepare_priority_level whether to search for a graylisted target to
  // probe. True if no whitelisted target has been found at a higher priority
  // level than the current one.
  bool _search_for_gray;

  // Where prepare_priority_level stores a graylisted target to probe, if one is
  // found.
  AddrInfo _unprobed_gray_target;
  bool _gray_found;

  // Where prepare_priority_level stores the whitelisted addresses and black and
  // graylisted addresses found respectively. Both are 2D jagged vectors,
  // containing a vector of AddrInfos for each SRV in the priority level.
  // get_from_priority_level processes the content in these to find targets and
  // _unhealthy_targets.
  //
  // Unhealthy is used as a catch-all term for blacklisted and graylisted
  // addresses, since graylisted addresses which are not being probed by this
  // request are treated the same as blacklisted addresses.
  std::vector<std::vector<AddrInfo> > _whitelisted_addresses_by_srv;
  std::vector<std::vector<AddrInfo> > _unhealthy_addresses_by_srv;

  // Vector where get_from_priority_level stores the black or graylisted targets
  // it finds when both whitelisted and blacklisted targets are desired.  Uses
  // this to add to targets if every whitelisted target has been returned and
  // still more are needed. If only whitelisted or only blacklisted targets were
  // requested then this vector is left empty.
  std::vector<AddrInfo> _unhealthy_targets;

  // The following 3 data members track the current position in various vectors
  // and maps of the iterator, allowing the lazy iterator to pause algorithms
  // once enough targets have been found, and resume the next time it's called.

  // The index of the SRV get_from_priority_level is currently looking at, so
  // that if the number of targets required is found before the entire priority
  // level is searched, get_from_priority_level can pause its search of the SRV
  // Records and resume from the same place if take is called again.
  int _current_srv;

  // The index of the unhealthy target to return next. Ensures that in
  // subsequent calls to take the same unhealthy target is not returned twice.
  int _unhealthy_target_pos;

  // An iterator to the priority level prepare_priority_level should look
  // at. Goes through all priority levels in order of highest to lowest
  // priority. Incremented by prepare_priority_level once it has finished
  // searching the current priority level.
  BaseResolver::SRVPriorityList::const_iterator _next_priority_level;
};
#endif
