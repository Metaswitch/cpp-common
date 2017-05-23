/**
 * @file baseresolver.cpp  Implementation of base class for DNS resolution.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <time.h>

#include <algorithm>
#include <sstream>
#include <iomanip>

#include "log.h"
#include "utils.h"
#include "baseresolver.h"
#include "sas.h"
#include "sasevent.h"
#include "weightedselector.h"

void PrintTo(const AddrInfo& ai, std::ostream* os)
{
  *os << ai.to_string();
}

BaseResolver::BaseResolver(DnsCachedResolver* dns_client) :
  _naptr_factory(),
  _naptr_cache(),
  _srv_factory(),
  _srv_cache(),
  _hosts(),
  _dns_client(dns_client)
{
}

BaseResolver::~BaseResolver()
{
}

// Removes all the entries from the blacklist.
void BaseResolver::clear_blacklist()
{
  TRC_DEBUG("Clear blacklist");
  pthread_mutex_lock(&_hosts_lock);
  _hosts.clear();
  pthread_mutex_unlock(&_hosts_lock);
}

// Creates the cache for storing NAPTR results.
void BaseResolver::create_naptr_cache(const std::map<std::string, int> naptr_services)
{
  // Create the NAPTR cache factory and the cache itself.
  TRC_DEBUG("Create NAPTR cache");
  _naptr_factory = new NAPTRCacheFactory(naptr_services, DEFAULT_TTL, _dns_client);
  _naptr_cache = new NAPTRCache(_naptr_factory);
}

/// Creates the cache for storing SRV results and selectors.
void BaseResolver::create_srv_cache()
{
  // Create the factory and cache for SRV.
  TRC_DEBUG("Create SRV cache");
  _srv_factory = new SRVCacheFactory(DEFAULT_TTL, _dns_client);
  _srv_cache = new SRVCache(_srv_factory);
}

/// Creates the blacklist of address/port/transport triplets.
void BaseResolver::create_blacklist(int blacklist_duration, int graylist_duration)
{
  // Create the blacklist (no factory required).
  TRC_DEBUG("Create black list");
  pthread_mutex_init(&_hosts_lock, NULL);
  _default_blacklist_duration = blacklist_duration;
  _default_graylist_duration = graylist_duration;
}

void BaseResolver::destroy_naptr_cache()
{
  TRC_DEBUG("Destroy NAPTR cache");
  delete _naptr_cache;
  delete _naptr_factory;
}

void BaseResolver::destroy_srv_cache()
{
  TRC_DEBUG("Destroy SRV cache");
  delete _srv_cache;
  delete _srv_factory;
}

void BaseResolver::destroy_blacklist()
{
  TRC_DEBUG("Destroy blacklist");
  _default_blacklist_duration = 0;
  _default_graylist_duration = 0;
  pthread_mutex_destroy(&_hosts_lock);
}

/// This algorithm selects a number of targets (IP address/port/transport
/// tuples) following the SRV selection algorithm in RFC2782, with a couple
/// of modifications.
/// -  Where SRV records resolve to multiple A/AAAA records, the SRVs at each
///    priority level are round-robined in the selected order, with IP
///    addresses chosen at random.  SRV at the next priority level are only
///    used when all A/AAAA records at higher riority levels have been used.
///    (This behaviour isn't specified in RFC2782, but is a corollary of the
///    requirements that
///    -  retries should initially be to different SRVs
///    -  servers at lower priority levels should not be used if servers from
///       a higher priority level are contactable.
/// -  Targets are checked against a blacklist.  Blacklisted targets are only
///    used if there are insufficient un-blacklisted targets.
///
void BaseResolver::srv_resolve(const std::string& srv_name,
                               int af,
                               int transport,
                               int retries,
                               std::vector<AddrInfo>& targets,
                               int& ttl,
                               SAS::TrailId trail)
{
  // Accumulate blacklisted targets in case they are needed.
  std::vector<AddrInfo> blacklisted_targets;

  // Clear the list of targets just in case.
  targets.clear();

  // Find/load the relevant SRV priority list from the cache.  This increments
  // a reference, so the list cannot be updated until we have finished with
  // it.
  SRVPriorityList* srv_list = _srv_cache->get(srv_name, ttl, trail);

  std::string targetlist_str;
  std::string blacklist_str;
  std::string added_from_blacklist_str;

  if (srv_list != NULL)
  {
    TRC_VERBOSE("SRV list found, %d priority levels", srv_list->size());

    // Select the SRV records in priority/weighted order.
    for (SRVPriorityList::const_iterator i = srv_list->begin();
         i != srv_list->end();
         ++i)
    {
      TRC_VERBOSE("Processing %d SRVs with priority %d", i->second.size(), i->first);

      std::vector<const SRV*> srvs;
      srvs.reserve(i->second.size());

      // Build a cumulative weighted tree for this priority level.
      WeightedSelector<SRV> selector(i->second);

      // Select entries while there are any with non-zero weights.
      while (selector.total_weight() > 0)
      {
        int ii = selector.select();
        TRC_DEBUG("Selected SRV %s:%d, weight = %d",
                  i->second[ii].target.c_str(),
                  i->second[ii].port,
                  i->second[ii].weight);
        srvs.push_back(&i->second[ii]);
      }

      // Do A/AAAA record look-ups for the selected SRV targets.
      std::vector<std::string> a_targets;
      std::vector<DnsResult> a_results;

      for (size_t ii = 0; ii < srvs.size(); ++ii)
      {
        a_targets.push_back(srvs[ii]->target);
      }
      TRC_VERBOSE("Do A record look-ups for %ld SRVs", a_targets.size());
      _dns_client->dns_query(a_targets,
                             (af == AF_INET) ? ns_t_a : ns_t_aaaa,
                             a_results,
                             trail);

      // Now form temporary lists for each SRV target containing the active
      // and blacklisted addresses, in randomized order.
      std::vector<std::vector<IP46Address> > active_addr(srvs.size());
      std::vector<std::vector<IP46Address> > blacklist_addr(srvs.size());

      for (size_t ii = 0; ii < srvs.size(); ++ii)
      {
        DnsResult& a_result = a_results[ii];
        TRC_DEBUG("SRV %s:%d returned %ld IP addresses",
                  srvs[ii]->target.c_str(),
                  srvs[ii]->port,
                  a_result.records().size());
        std::vector<IP46Address>& active = active_addr[ii];
        std::vector<IP46Address>& blacklist = blacklist_addr[ii];
        active.reserve(a_result.records().size());
        blacklist.reserve(a_result.records().size());

        for (size_t jj = 0; jj < a_result.records().size(); ++jj)
        {
          AddrInfo ai;
          ai.transport = transport;
          ai.port = srvs[ii]->port;
          ai.address = to_ip46(a_result.records()[jj]);

          if (!blacklisted(ai))
          {
            // Address isn't blacklisted, so copy across to the active list.
            active.push_back(ai.address);
          }
          else
          {
            // Address is blacklisted, so copy to blacklisted list.
            blacklist.push_back(ai.address);
          }

          // Take the smallest ttl returned so far.
          ttl = std::min(ttl, a_result.ttl());
        }

        // Randomize the order of both vectors.
        std::random_shuffle(active.begin(), active.end());
        std::random_shuffle(blacklist.begin(), blacklist.end());
      }

      // Finally select the appropriate number of targets by looping through
      // the SRV records taking one address each time until either we have
      // enough for the number of retries allowed, or we have no more addresses.
      bool more = true;
      while ((targets.size() < (size_t)retries) &&
             (more))
      {
        more = false;
        AddrInfo ai;
        ai.transport = transport;

        for (size_t ii = 0;
             (ii < srvs.size()) && (targets.size() < (size_t)retries);
             ++ii)
        {
          ai.port = srvs[ii]->port;
          ai.priority = srvs[ii]->priority;
          ai.weight = srvs[ii]->weight;

          if (!active_addr[ii].empty())
          {
            ai.address = active_addr[ii].back();
            active_addr[ii].pop_back();
            targets.push_back(ai);
            std::string target = ai.address_and_port_to_string();
            std::string tg = "[" + target + "] ";
            targetlist_str = targetlist_str + tg;

            TRC_VERBOSE("Added a server, now have %ld of %d", targets.size(), retries);
          }

          if (!blacklist_addr[ii].empty())
          {
            ai.address = blacklist_addr[ii].back();
            blacklist_addr[ii].pop_back();
            blacklisted_targets.push_back(ai);
            std::string blacklistee = ai.address_and_port_to_string();
            std::string bl = "[" + blacklistee + "] ";
            blacklist_str = blacklist_str + bl;
          }

          more = more || ((!active_addr[ii].empty()) || (!blacklist_addr[ii].empty()));
        }

      }

      if (targets.size() >= (size_t)retries)
      {
        // We have enough targets so don't move to the next priority level.
        break;
      }
    }

    // If we've gone through the whole set of SRVs and haven't found enough
    // unblacklisted targets, add blacklisted targets.
    if (targets.size() < (size_t)retries)
    {
      size_t to_copy = (size_t)retries - targets.size();

      if (to_copy > blacklisted_targets.size())
      {
        to_copy = blacklisted_targets.size();
      }

      TRC_VERBOSE("Adding %ld servers from blacklist", to_copy);

      for (size_t ii = 0; ii < to_copy; ++ii)
      {
        targets.push_back(blacklisted_targets[ii]);
        std::string blacklistee = blacklisted_targets[ii].address_and_port_to_string();
        std::string bl = "[" + blacklistee + "]";
        added_from_blacklist_str = added_from_blacklist_str + bl;
      }
    }
  }

  if (trail != 0)
  {
    SAS::Event event(trail, SASEvent::BASERESOLVE_SRV_RESULT, 0);
    event.add_var_param(srv_name);
    event.add_var_param(targetlist_str);
    event.add_var_param(blacklist_str);
    event.add_var_param(added_from_blacklist_str);
    SAS::report_event(event);
  }

  _srv_cache->dec_ref(srv_name);
}

/// Does A/AAAA record queries for the specified hostname.
void BaseResolver::a_resolve(const std::string& hostname,
                             int af,
                             int port,
                             int transport,
                             int retries,
                             std::vector<AddrInfo>& targets,
                             int& ttl,
                             SAS::TrailId trail)
{
  BaseAddrIterator* it = a_resolve_iter(hostname, af, port, transport, ttl, trail);
  targets = it->take(retries);
  delete it; it = nullptr;
}

BaseAddrIterator* BaseResolver::a_resolve_iter(const std::string& hostname,
                                               int af,
                                               int port,
                                               int transport,
                                               int& ttl,
                                               SAS::TrailId trail)
{
  DnsResult result = _dns_client->dns_query(hostname, (af == AF_INET) ? ns_t_a : ns_t_aaaa, trail);
  ttl = result.ttl();

  TRC_DEBUG("Found %ld A/AAAA records, creating iterator", result.records().size());

  return new LazyAddrIterator(result, this, port, transport, trail);
}

/// Converts a DNS A or AAAA record to an IP46Address structure.
IP46Address BaseResolver::to_ip46(const DnsRRecord* rr)
{
  IP46Address addr;
  if (rr->rrtype() == ns_t_a)
  {
    // A record.
    DnsARecord* ar = (DnsARecord*)rr;
    addr.af = AF_INET;
    addr.addr.ipv4 = ar->address();
  }
  else
  {
    // AAAA record.
    DnsAAAARecord* ar = (DnsAAAARecord*)rr;
    addr.af = AF_INET6;
    addr.addr.ipv6 = ar->address();
  }

  return addr;
}

/// Adds an address, port, transport tuple to the blacklist.
void BaseResolver::blacklist(const AddrInfo& ai,
                             int blacklist_ttl,
                             int graylist_ttl)
{
  std::string ai_str = ai.to_string();
  TRC_DEBUG("Add %s to blacklist for %d seconds, graylist for %d seconds",
            ai_str.c_str(), blacklist_ttl, graylist_ttl);
  pthread_mutex_lock(&_hosts_lock);
  _hosts.erase(ai);
  _hosts.emplace(ai, Host(blacklist_ttl, graylist_ttl));
  pthread_mutex_unlock(&_hosts_lock);
}

bool BaseResolver::blacklisted(const AddrInfo& ai)
{
  bool rc = false;

  pthread_mutex_lock(&_hosts_lock);

  rc = (host_state(ai) == Host::State::BLACK);

  pthread_mutex_unlock(&_hosts_lock);

  return rc;
}

/// Parses a target as if it was an IPv4 or IPv6 address and returns the
/// status of the parse.
bool BaseResolver::parse_ip_target(const std::string& target, IP46Address& address)
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

BaseResolver::NAPTRCacheFactory::NAPTRCacheFactory(const std::map<std::string, int>& services,
                                                   int default_ttl,
                                                   DnsCachedResolver* dns_client) :
  _services(services),
  _default_ttl(default_ttl),
  _dns_client(dns_client)
{
}

BaseResolver::NAPTRCacheFactory::~NAPTRCacheFactory()
{
}

BaseResolver::NAPTRReplacement* BaseResolver::NAPTRCacheFactory::get(std::string key, int& ttl, SAS::TrailId trail)
{
  // Iterate NAPTR lookups starting with querying the target domain until
  // we get a terminal result.
  TRC_DEBUG("NAPTR cache factory called for %s", key.c_str());
  NAPTRReplacement* repl = NULL;
  std::string query_key = key;
  int expires = 0;
  bool loop_again = true;

  while (loop_again)
  {
    // Assume this is our last loop - we'll correct this if we find we should go round again.
    loop_again = false;

    // Issue the NAPTR query.
    TRC_DEBUG("Sending DNS NAPTR query for %s", query_key.c_str());
    DnsResult result = _dns_client->dns_query(query_key, ns_t_naptr, trail);

    if (!result.records().empty())
    {
      // Process the NAPTR records as per RFC2915 section 4.  First step is to
      // collect the records that match one of the requested services and have
      // acceptable flags.
      std::vector<DnsNaptrRecord*> filtered;

      for (std::vector<DnsRRecord*>::const_iterator i = result.records().begin();
           i != result.records().end();
           ++i)
      {
        DnsNaptrRecord* naptr = (DnsNaptrRecord*)(*i);
        if ((_services.find(naptr->service()) != _services.end()) &&
            ((strcasecmp(naptr->flags().c_str(), "S") == 0) ||
             (strcasecmp(naptr->flags().c_str(), "A") == 0) ||
             (strcasecmp(naptr->flags().c_str(), "") == 0)))
        {
          // This record has passed the filter.
          filtered.push_back(naptr);
        }
      }

      // Now sort the resulting filtered list on order and preference.
      std::sort(filtered.begin(),
                filtered.end(),
                BaseResolver::NAPTRCacheFactory::compare_naptr_order_preference);

      // Now loop through the records looking for the first match - where a
      // match is either a record with a replacement string, or a regular
      // expression which matches the input.  Note that we must always use
      // the originally specified domain in the regex - using the result of
      // a previous regex application is not allowed.
      for (size_t ii = 0; ii < filtered.size(); ++ii)
      {
        DnsNaptrRecord* naptr = filtered[ii];
        std::string replacement = naptr->replacement();

        if ((replacement == "") &&
            (naptr->regexp() != ""))
        {
          // Record has no replacement value, but does have a regular expression.
          boost::regex regex;
          std::string replace;
          if (parse_regex_replace(naptr->regexp(), regex, replace))
          {
            // We have a valid regex so try to match it to the original
            // queried domain to generate a new key.  Note that going straight
            // to replace is okay.  We haven't set the format_no_copy flag so
            // parts of the string that do not match the regex will not be
            // copied, so no match means we end up with an empty string.
            replacement = boost::regex_replace(key,
                                               regex,
                                               replace,
                                               boost::regex_constants::format_first_only);
          }
        }

        // Update ttl with the expiry of this record, so if we do get
        // a positive result we only cache it while all of the NAPTR records
        // we followed are still valid.
        if ((expires == 0) ||
            (expires > naptr->expires()))
        {
          expires = naptr->expires();
        }

        if (replacement != "")
        {
          // We have a match, so we can terminate processing here and look
          // at the flags to decide what to do next.
          if (strcasecmp(naptr->flags().c_str(), "") == 0)
          {
            // We need to iterate the NAPTR query.
            query_key = replacement;
            loop_again = true;
          }
          else
          {
            // This is a terminal record, so set up the result.
            repl = new NAPTRReplacement;
            repl->replacement = replacement;
            repl->flags = naptr->flags();
            repl->transport = _services[naptr->service()];
            ttl = expires - time(NULL);
          }
          break;
        }
      }
    }
    else
    {
      // No NAPTR record found, so return no entry with the default TTL.
      ttl = _default_ttl;
    }
  }
  return repl;
}

void BaseResolver::NAPTRCacheFactory::evict(std::string key, NAPTRReplacement* value)
{
  TRC_DEBUG("Evict NAPTR cache %s", key.c_str());
  delete value;
}

bool BaseResolver::NAPTRCacheFactory::parse_regex_replace(const std::string& regex_replace,
                                                          boost::regex& regex,
                                                          std::string& replace)
{
  bool success = false;

  // Split the regular expression into the match and replace sections.  RFC3402
  // says any character other than 1-9 or i can be the delimiter, but
  // recommends / or !.  We just use the first character and reject if it
  // doesn't neatly split the regex into two.
  std::vector<std::string> match_replace;
  Utils::split_string(regex_replace, regex_replace[0], match_replace);

  if (match_replace.size() == 2)
  {
    TRC_DEBUG("Split regex into match=%s, replace=%s", match_replace[0].c_str(), match_replace[1].c_str());
    try
    {
      regex.assign(match_replace[0]);
      replace = match_replace[1];
      success = true;
    }
    // LCOV_EXCL_START
    catch (...)
    {
      success = false;
    }
    // LCOV_EXCL_STOP
  }
  else
  {
    success = false;
  }

  return success;
}


bool BaseResolver::NAPTRCacheFactory::compare_naptr_order_preference(DnsNaptrRecord* r1,
                                                                     DnsNaptrRecord* r2)
{
  return ((r1->order() < r2->order()) ||
          ((r1->order() == r2->order()) &&
           (r1->preference() < r2->preference())));
}


BaseResolver::SRVCacheFactory::SRVCacheFactory(int default_ttl,
                                               DnsCachedResolver* dns_client) :
  _default_ttl(default_ttl),
  _dns_client(dns_client)
{
}

BaseResolver::SRVCacheFactory::~SRVCacheFactory()
{
}

BaseResolver::SRVPriorityList* BaseResolver::SRVCacheFactory::get(std::string key, int& ttl, SAS::TrailId trail)
{
  TRC_DEBUG("SRV cache factory called for %s", key.c_str());
  SRVPriorityList* srv_list = NULL;

  DnsResult result = _dns_client->dns_query(key, ns_t_srv, trail);

  if (!result.records().empty())
  {
    // We have a result.
    TRC_DEBUG("SRV query returned %d records", result.records().size());
    srv_list = new SRVPriorityList;
    ttl = result.ttl();

    // Sort the records on priority.
    std::sort(result.records().begin(), result.records().end(), compare_srv_priority);

    // Now rearrange the results in to an SRV priority list (a map of vectors
    // for each priority level).
    for (std::vector<DnsRRecord*>::const_iterator i = result.records().begin();
         i != result.records().end();
         ++i)
    {
      DnsSrvRecord* srv_record = (DnsSrvRecord*)(*i);

      // Get the appropriate priority list of SRVs.
      std::vector<SRV>& plist = (*srv_list)[srv_record->priority()];

      // Add a new entry for this SRV.
      plist.push_back(SRV());
      SRV& srv = plist.back();
      srv.target = srv_record->target();
      srv.port = srv_record->port();
      srv.priority = srv_record->priority();
      srv.weight = srv_record->weight();

      // Adjust the weight.  Any items which have weight 0 are increase to
      // weight of one, and non-zero weights are multiplied by 100.  This gives
      // the right behaviour as per RFC2782 - when all weights are zero we
      // round-robin (but still have the ability to blacklist) and when there
      // are non-zero weights the zero weighted items have a small (but not
      // specified in RFC2782) chance of selection.
      srv.weight = (srv.weight == 0) ? 1 : srv.weight * 100;
    }
  }
  else
  {
    // No results from SRV query, so return no entry with the default TTL
    ttl = _default_ttl;
  }

  return srv_list;
}

void BaseResolver::SRVCacheFactory::evict(std::string key, SRVPriorityList* value)
{
  TRC_DEBUG("Evict SRV cache %s", key.c_str());
  delete value;
}

bool BaseResolver::SRVCacheFactory::compare_srv_priority(DnsRRecord* r1,
                                                         DnsRRecord* r2)
{
  return (((DnsSrvRecord*)r1)->priority() < ((DnsSrvRecord*)r2)->priority());
}

BaseResolver::Host::Host(int blacklist_ttl,int graylist_ttl) :
  _being_probed(false)
{
  time_t current_time = time(NULL);
  _blacklist_expiry_time = current_time + blacklist_ttl;
  _graylist_expiry_time = current_time + blacklist_ttl + graylist_ttl;
}

BaseResolver::Host::~Host()
{
}

std::string BaseResolver::Host::state_to_string(State state)
{
  switch(state)
  {
  case State::WHITE:
    return "WHITE";
  case State::GRAY_NOT_PROBING:
    return "GRAY_NOT_PROBING";
  case State::GRAY_PROBING:
    return "GRAY_PROBING";
  case State::BLACK:
    return "BLACK";
    // LCOV_EXCL_START
  default:
    return "UNKNOWN";
    // LCOV_EXCL_STOP
  }
}

BaseResolver::Host::State BaseResolver::Host::get_state(time_t current_time)
{
  if (current_time < _blacklist_expiry_time)
  {
    return State::BLACK;
  }
  else if (current_time < _graylist_expiry_time)
  {
    if (_being_probed)
    {
      return State::GRAY_PROBING;
    }
    else
    {
      return State::GRAY_NOT_PROBING;
    }
  }
  else
  {
    return State::WHITE;
  }
}

void BaseResolver::Host::success()
{
  if (get_state() != State::BLACK)
  {
    _being_probed = false;
    _blacklist_expiry_time = 0;
    _graylist_expiry_time = 0;
  }
}

void BaseResolver::Host::selected_for_probing(pthread_t user_id)
{
  if (get_state() == State::GRAY_NOT_PROBING)
  {
    _being_probed = true;
    _probing_user_id = user_id;
  }
}

void BaseResolver::Host::untested(pthread_t user_id)
{
  if ((get_state() == State::GRAY_PROBING) &&
      (user_id == _probing_user_id))
  {
    _being_probed = false;
  }
}

BaseResolver::Host::State BaseResolver::host_state(const AddrInfo& ai, time_t current_time)
{
  Host::State state;
  Hosts::iterator i = _hosts.find(ai);
  std::string ai_str;

  if (Log::enabled(Log::DEBUG_LEVEL))
  {
    ai_str = ai.to_string();
  }

  if (i != _hosts.end())
  {
    state = i->second.get_state(current_time);
    if (state == Host::State::WHITE)
    {
      TRC_DEBUG("%s graylist time elapsed", ai_str.c_str());
      _hosts.erase(i);
    }
  }
  else
  {
    state = Host::State::WHITE;
  }

  if (Log::enabled(Log::DEBUG_LEVEL))
  {
    std::string state_str = Host::state_to_string(state);
    TRC_DEBUG("%s has state: %s", ai_str.c_str(), state_str.c_str());
  }

  return state;
}

void BaseResolver::success(const AddrInfo& ai)
{
  if (Log::enabled(Log::DEBUG_LEVEL))
  {
    std::string ai_str = ai.to_string();
    TRC_DEBUG("Successful response from  %s", ai_str.c_str());
  }

  pthread_mutex_lock(&_hosts_lock);

  Hosts::iterator i = _hosts.find(ai);

  if (i != _hosts.end())
  {
    i->second.success();
  }

  pthread_mutex_unlock(&_hosts_lock);
}

void BaseResolver::select_for_probing(const AddrInfo& ai)
{
  Hosts::iterator i = _hosts.find(ai);

  if (i != _hosts.end())
  {
    std::string ai_str = ai.to_string();
    TRC_DEBUG("%s selected for probing", ai_str.c_str());
    i->second.selected_for_probing(pthread_self());
  }
}

void BaseResolver::untested(const AddrInfo& ai)
{
  std::string ai_str = ai.to_string();
  TRC_DEBUG("%s returned untested", ai_str.c_str());

  pthread_mutex_lock(&_hosts_lock);
  Hosts::iterator i = _hosts.find(ai);

  if (i != _hosts.end())
  {
    i->second.untested(pthread_self());
  }

  pthread_mutex_unlock(&_hosts_lock);
}

bool BaseAddrIterator::next(AddrInfo &target)
{
  bool value_set;
  std::vector<AddrInfo> next_one = take(1);

  if (!next_one.empty())
  {
    target = next_one.front();
    value_set = true;
  }
  else
  {
    value_set = false;
  }

  return value_set;
}

std::vector<AddrInfo> SimpleAddrIterator::take(int num_requested_targets)
{
  // Determine the number of elements to be returned, and create an iterator
  // pointing to the location at which to split _targets.
  int num_targets_to_return = std::min(num_requested_targets, int(_targets.size()));
  std::vector<AddrInfo>::iterator targets_it = _targets.begin();
  std::advance(targets_it, num_targets_to_return);

  // Set targets to the first actual_num_requested_targets elements of _targets, and
  // remove those elements from _targets.
  std::vector<AddrInfo> targets(_targets.begin(), targets_it);
  _targets = std::vector<AddrInfo>(targets_it, _targets.end());

  return targets;
}

LazyAddrIterator::LazyAddrIterator(DnsResult& dns_result,
                                   BaseResolver* resolver,
                                   int port,
                                   int transport,
                                   SAS::TrailId trail) :
  _resolver(resolver),
  _trail(trail),
  _first_call(true)
{
  _hostname = dns_result.domain();

  AddrInfo ai;
  ai.port = port;
  ai.transport = transport;

  // Reserve memory for the unused results vector to avoid reallocation.
  _unused_results.reserve(dns_result.records().size());

  // Create an AddrInfo object for each returned DnsRRecord, and add them to the
  // query results vector.
  for (std::vector<DnsRRecord*>::const_iterator result_it = dns_result.records().begin();
       result_it != dns_result.records().end();
       ++result_it)
  {
    ai.address = _resolver->to_ip46(*result_it);
    _unused_results.push_back(ai);
  }

  // Shuffle the results for load balancing purposes
  std::random_shuffle(_unused_results.begin(), _unused_results.end());
}

std::vector<AddrInfo> LazyAddrIterator::take(int num_requested_targets)
{
  // Vector of targets to be returned
  std::vector<AddrInfo> targets;

  // Strings for logging purposes.
  std::string graylisted_targets_str;
  std::string whitelisted_targets_str;
  std::string blacklisted_targets_str;

  std::string found_blacklisted_str;

  // This lock must be held to safely call the host_state method of
  // BaseResolver.
  pthread_mutex_lock(&(_resolver->_hosts_lock));

  // If there are any graylisted records, the Iterator should return one first,
  // and then no more.
  if (_first_call)
  {
    _first_call = false;

    // Iterate over the records. Since the records are in random order anyway,
    // iterate in reverse order; it is faster to remove elements from near the
    // end of the vector.
    for (std::vector<AddrInfo>::reverse_iterator result_it = _unused_results.rbegin();
         result_it != _unused_results.rend();
         ++result_it)
    {
      if (_resolver->host_state(*result_it) == BaseResolver::Host::State::GRAY_NOT_PROBING)
      {
        // Add the record to the targets list.
        _resolver->select_for_probing(*result_it);
        targets.push_back(*result_it);

        // Update logging.
        graylisted_targets_str += result_it->address_and_port_to_string() + ";";
        TRC_DEBUG("Added a graylisted server, now have %ld of %d",
                  targets.size(),
                  num_requested_targets);

        // Remove the record from the results list.
        _unused_results.erase(std::next(result_it).base());
        break;
      }
    }
  }

  // Add whitelisted records to the vector of targets to return, and unhealthy
  // records to the unhealthy results vector. Targets should be added until the
  // required number is reached, or the unused results are exhausted.
  while ((_unused_results.size() > 0) && (targets.size() < (size_t)num_requested_targets))
  {
    AddrInfo result = _unused_results.back();
    _unused_results.pop_back();

    if (_resolver->host_state(result) == BaseResolver::Host::State::WHITE)
    {
      // Add the record to the targets list.
      targets.push_back(result);

      // Update logging.
      whitelisted_targets_str += result.address_and_port_to_string() + ";";
      TRC_DEBUG("Added a whitelisted server, now have %ld of %d",
                targets.size(),
                num_requested_targets);
    }
    else
    {
      // Add the record to the list of unhealthy targets.
      _unhealthy_results.push_back(result);

      // Update logging.
      found_blacklisted_str += result.address_and_port_to_string() + ";";
    }
  }

  pthread_mutex_unlock(&(_resolver->_hosts_lock));

  // If the targets vector does not yet contain enough targets, add unhealthy
  // targets
  while ((_unhealthy_results.size() > 0) && (targets.size() < (size_t)num_requested_targets))
  {
    AddrInfo result = _unhealthy_results.back();
    _unhealthy_results.pop_back();

    // Add the record to the targets list
    targets.push_back(result);

    // Update logging.
    blacklisted_targets_str += result.address_and_port_to_string() + ";";
    TRC_DEBUG("Added an unhealthy server, now have %ld of %d",
              targets.size(),
              num_requested_targets);
  }

  if (_trail != 0)
  {
    SAS::Event event(_trail, SASEvent::BASERESOLVE_A_RESULT_TARGET_SELECT, 0);
    event.add_var_param(_hostname);
    event.add_var_param(graylisted_targets_str);
    event.add_var_param(whitelisted_targets_str);
    event.add_var_param(blacklisted_targets_str);
    event.add_var_param(found_blacklisted_str);
    SAS::report_event(event);
  }

  return targets;
}

