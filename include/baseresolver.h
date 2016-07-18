/**
 * @file baseresolver.h  Declaration of base class for DNS resolution.
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

#ifndef BASERESOLVER_H__
#define BASERESOLVER_H__

#include <list>
#include <map>
#include <vector>
#include <boost/regex.hpp>
#include <pthread.h>

#include "log.h"
#include "dnscachedresolver.h"
#include "ttlcache.h"
#include "utils.h"
#include "sas.h"

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
};

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

  void blacklist(const AddrInfo& ai)
  {
    blacklist(ai, _default_blacklist_duration, _default_graylist_duration);
  }
  void blacklist(const AddrInfo& ai, int blacklist_ttl)
  {
    blacklist(ai, blacklist_ttl, _default_graylist_duration);
  }
  void blacklist(const AddrInfo& ai, int blacklist_ttl, int graylist_ttl);
  bool blacklisted(const AddrInfo& ai);

  /// Utility function to parse a target name to see if it is a valid IPv4 or IPv6 address.
  bool parse_ip_target(const std::string& target, IP46Address& address);

  void clear_blacklist();
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
  // appropriate targets.
  void srv_resolve(const std::string& srv_name,
                   int af,
                   int transport,
                   int retries,
                   std::vector<AddrInfo>& targets,
                   int& ttl,
                   SAS::TrailId trail);

  /// Does an A/AAAA record resolution for the specified name, selecting
  /// appropriate targets.
  void a_resolve(const std::string& name,
                 int af,
                 int port,
                 int transport,
                 int retries,
                 std::vector<AddrInfo>& targets,
                 int& ttl,
                 SAS::TrailId trail);

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

  /// The SRVWeightedSelector class is a temporary class used to implemented
  /// selection of SRV records at a single priority level according to the
  /// weighting of each record.
  class SRVWeightedSelector
  {
  public:
    /// Constructor.
    SRVWeightedSelector(const std::vector<SRV>& srvs);

    /// Destructor.
    ~SRVWeightedSelector();

    /// Renders the current state of the tree as a string.
    std::string to_string() const;

    /// Selects an entry and sets its weight to zero.
    int select();

    /// Returns the current total weight of the items in the selector.
    int total_weight();

  private:
    std::vector<int> _tree;
  };

  /// The global blacklist holds a list of transport/IP address/port
  /// combinations which have been blacklisted because the destination is
  /// unresponsive (either TCP connection attempts are failing or a UDP
  /// destination is unreachable).

  /// Private class to hold data and methods associated to a transport/IP
  /// address/port combination in the blacklist system. Each Host is associated
  /// with exactly one combination.
  class Host
  {
  public:
    /// Constructor
    Host(int blacklist_ttl, int graylist_ttl);

    /// Destructor
    ~Host();

    /// Enum representing the current state of a Host in the blacklist
    /// system. Whitelisted, graylisted and not being probed, graylisted and
    /// being probed, and blacklisted respectively.
    enum struct State {WHITE, GRAY_NOT_PROBING, GRAY_PROBING, BLACK};
    /// Returns a string representation of the given state.
    static std::string state_to_string(State state);
    /// Returns the current state of a Host.
    State get_state(time_t current_time);

    /// Indicates that the combination associated with this Host has been
    /// successfully contacted.
    void success();
    /// Indicates that the combination associated with this Host is being probed
    /// by the given user.
    void probing(pthread_t user_id);
    /// Indicates that the combination associated with this Host has gone
    /// untested by the given user.
    void untested(pthread_t user_id);

  private:
    /// The time at which this Host is to be removed from the blacklist and
    /// placed onto the graylist.
    time_t _blacklist_expiry_time;
    /// The time at which this Host is to be removed from the graylist.
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

public:
  /// Indicates that the AddrInfo has responded.
  void success(const AddrInfo& ai);
  /// Indicates that the calling thread has left the AddrInfo untested.
  void untested(const AddrInfo& ai);

protected:
  /// Indicates that the calling thread is probing theg given AddrInfo.
  /// _hosts_lock must be held when calling this method.
  void probing(const AddrInfo& ai);

  int _default_blacklist_duration;
  int _default_graylist_duration;

  /// Stores a pointer to the DNS client this resolver should use.
  DnsCachedResolver* _dns_client;

  static const int DEFAULT_TTL = 300;

};

#endif
