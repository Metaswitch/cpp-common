/**
 * @file dnscachedresolver.h Definitions for the DNS caching resolver.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef DNSCACHEDRESOLVER_H__
#define DNSCACHEDRESOLVER_H__

#include <string.h>
#include <pthread.h>
#include <time.h>

#include <map>
#include <list>
#include <vector>
#include <memory>

#include <arpa/nameser.h>
#include <ares.h>

#include "utils.h"
#include "dnsrrecords.h"
#include "sas.h"

class DnsResult
{
public:
  DnsResult(const std::string& domain, int dnstype, const std::vector<DnsRRecord*>& records, int ttl);
  DnsResult(const std::string& domain, int dnstype, int ttl);
  DnsResult(const DnsResult &obj);
  DnsResult(DnsResult &&obj);
  ~DnsResult();

  const std::string& domain() const { return _domain; }
  int dnstype() const { return _dnstype; }
  std::vector<DnsRRecord*>& records() { return _records; }
  int ttl() const { return _ttl; }

private:
  std::string _domain;
  int _dnstype;
  std::vector<DnsRRecord*> _records;
  int _ttl;
};

class DnsCachedResolver
{
public:
  DnsCachedResolver(const std::vector<IP46Address>& dns_servers, const std::string& filename = "");
  DnsCachedResolver(const std::vector<std::string>& dns_servers, const std::string& filename = "");
  DnsCachedResolver(const std::string& dns_server, int port, const std::string& filename = "");
  DnsCachedResolver(const std::string& dns_server) : DnsCachedResolver(dns_server, 53) {};
  ~DnsCachedResolver();

  /// Queries a single DNS record.
  DnsResult dns_query(const std::string& domain,
                      int dnstype,
                      SAS::TrailId trail);

  /// Queries multiple DNS records in parallel.
  void dns_query(const std::vector<std::string>& domains,
                 int dnstype,
                 std::vector<DnsResult>& results,
                 SAS::TrailId trail);

  /// Adds or updates an entry in the cache.
  void add_to_cache(const std::string& domain,
                    int dnstype,
                    std::vector<DnsRRecord*>& records);

  /// Display the current status of the cache.
  std::string display_cache();

  /// Clear the cache
  void clear();

  // Reads DNS records from _dns_config_file and stores them in _static_records
  void reload_static_records();

private:
  void init(const std::vector<IP46Address>& dns_server);
  void init_from_server_ips(const std::vector<std::string>& dns_server);

  struct DnsChannel
  {
    ares_channel channel;
    DnsCachedResolver* resolver;
    int pending_queries;
  };

  class DnsTsx
  {
  public:
    DnsTsx(DnsChannel* channel, const std::string& domain, int dnstype, SAS::TrailId trail);
    ~DnsTsx();;
    void execute();
    static void ares_callback(void* arg, int status, int timeouts, unsigned char* abuf, int alen);
    void ares_callback(int status, int timeouts, unsigned char* abuf, int alen);

  private:
    DnsChannel* _channel;
    std::string _domain;
    int _dnstype;
    SAS::TrailId _trail;
  };

  struct DnsCacheEntry
  {
    bool pending_query;
    std::string domain;
    int dnstype;
    int expires;
    std::vector<DnsRRecord*> records;
  };

  class DnsCacheKeyCompare
  {
  public:
    bool operator()(const std::pair<int, std::string> lhs, const std::pair<int, std::string> rhs)
    {
      if (lhs.first > rhs.first)
      {
        return true;
      }
      else if (lhs.first < rhs.first)
      {
        return false;
      }
      else
      {
        // DNSTYPE is identical, so do case insensitive string compare.
        return strcasecmp(lhs.second.c_str(), rhs.second.c_str()) > 0;
      }
    }
  };

  typedef std::shared_ptr<DnsCacheEntry> DnsCacheEntryPtr;
  typedef std::pair<int, std::string> DnsCacheKey;
  typedef std::multimap<int, DnsCacheKey> DnsCacheExpiryList;
  typedef std::map<DnsCacheKey,
                   DnsCacheEntryPtr,
                   DnsCacheKeyCompare> DnsCache;

  /// Performs the actual DNS query.
  void inner_dns_query(const std::vector<std::string>& domains,
                       int dnstype,
                       std::vector<DnsResult>& results,
                       SAS::TrailId trail);

  void dns_response(const std::string& domain,
                    int dnstype,
                    int status,
                    unsigned char* abuf,
                    int alen,
                    SAS::TrailId trail);

  bool caching_enabled(int rrtype);

  DnsCacheEntryPtr get_cache_entry(const std::string& domain, int dnstype);
  DnsCacheEntryPtr create_cache_entry(const std::string& domain, int dnstype);
  void add_to_expiry_list(DnsCacheEntryPtr ce);
  void expire_cache();
  void add_record_to_cache(DnsCacheEntryPtr ce, DnsRRecord* rr);
  void clear_cache_entry(DnsCacheEntryPtr ce);


  DnsChannel* get_dns_channel();
  void wait_for_replies(DnsChannel* channel);
  static void destroy_dns_channel(DnsChannel* channel);

  struct ares_addr_node _ares_addrs[3];
  std::vector<IP46Address> _dns_servers;
  int _port;

  // The thread-local store - used for storing DnsChannels.
  pthread_key_t _thread_local;

  /// The cache itself is held in a map indexed on RRTYPE and RRNAME, and a
  /// multimap indexed on expiry time.
  pthread_mutex_t _cache_lock;
  pthread_cond_t _got_reply_cond;
  DnsCache _cache;

  std::string _dns_config_file;
  std::map<std::string, std::vector<DnsRRecord*>> _static_records;

  // Expiry is done efficiently by storing pointers to cache entries in a
  // multimap indexed on expiry time.
  DnsCacheExpiryList _cache_expiry_list;

  /// The default negative cache period is set to 5 minutes.
  /// @TODO - may make sense for this to be configured, or even different for
  /// each record type.
  static const int DEFAULT_NEGATIVE_CACHE_TTL = 300;

  /// The time to keep records after they expire before freeing them.
  /// This provides a grace period if a DNS server becomes temporarily
  /// unresponsive, but doesn't risk leaking memory.
  static const int EXTRA_INVALID_TIME = 300;
};

#endif
