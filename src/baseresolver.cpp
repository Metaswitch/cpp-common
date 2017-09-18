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
/// tuples) following the SRV selection algorithm in RFC2782, with a couple of
/// modifications.
/// -  Where SRV records resolve to multiple A/AAAA records, the SRVs at each
///    priority level are round-robined in a randomly selected order according
///    to the weights, with IP addresses chosen at random.  SRV at the next
///    priority level are only used when all A/AAAA records at higher priority
///    levels have been used.
///    (This behaviour isn't specified in RFC2782, but is a corollary of the
///    requirements that
///    -  retries should initially be to different SRVs
///    -  servers at lower priority levels should not be used if servers from a
///       higher priority level are contactable.)
/// -  Targets are checked against a blacklist. Blacklisted targets are only
///    used if there are insufficient un-blacklisted targets.
/// -  Targets are checked against a graylist. If there are unprobed graylisted
///    targets at the highest priority level then exactly one of them will
///    always be probed as the first target. This takes precedence over
///    respecting the weights. Any remaining graylisted targets will be treated
///    as though they were blacklisted.
void BaseResolver::srv_resolve(const std::string& srv_name,
                               int af,
                               int transport,
                               int retries,
                               std::vector<AddrInfo>& targets,
                               int& ttl,
                               SAS::TrailId trail,
                               int allowed_host_state)
{
  LazySRVResolveIter* targets_iter = (LazySRVResolveIter*) srv_resolve_iter(srv_name, af, transport, trail, allowed_host_state);
  targets = targets_iter->take(retries);
  ttl = targets_iter->get_min_ttl();
  delete targets_iter; targets_iter = nullptr;
}

BaseAddrIterator* BaseResolver::srv_resolve_iter(const std::string& srv_name,
                                                 int af,
                                                 int transport,
                                                 SAS::TrailId trail,
                                                 int allowed_host_state)
{
  TRC_DEBUG("Creating a lazy iterator for SRV Resolution");
  return new LazySRVResolveIter(_dns_client,
                                this,
                                af,
                                transport,
                                srv_name,
                                trail,
                                allowed_host_state);
}

/// Does A/AAAA record queries for the specified hostname.
void BaseResolver::a_resolve(const std::string& hostname,
                             int af,
                             int port,
                             int transport,
                             int retries,
                             std::vector<AddrInfo>& targets,
                             int& ttl,
                             SAS::TrailId trail,
                             int allowed_host_state)
{
  BaseAddrIterator* it = a_resolve_iter(hostname, af, port, transport, ttl, trail, allowed_host_state);
  targets = it->take(retries);
  delete it; it = nullptr;
}

BaseAddrIterator* BaseResolver::a_resolve_iter(const std::string& hostname,
                                               int af,
                                               int port,
                                               int transport,
                                               int& ttl,
                                               SAS::TrailId trail,
                                               int allowed_host_state)
{
  DnsResult result = _dns_client->dns_query(hostname, (af == AF_INET) ? ns_t_a : ns_t_aaaa, trail);
  ttl = result.ttl();

  TRC_DEBUG("Found %ld A/AAAA records, creating iterator", result.records().size());

  return new LazyAResolveIter(result, this, port, transport, trail, allowed_host_state);
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

std::shared_ptr<BaseResolver::NAPTRReplacement> BaseResolver::NAPTRCacheFactory::get(std::string key, int& ttl, SAS::TrailId trail)
{
  // Iterate NAPTR lookups starting with querying the target domain until
  // we get a terminal result.
  TRC_DEBUG("NAPTR cache factory called for %s", key.c_str());
  std::shared_ptr<NAPTRReplacement> repl = nullptr;
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
            repl = std::make_shared<NAPTRReplacement>();
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

std::shared_ptr<BaseResolver::SRVPriorityList> BaseResolver::SRVCacheFactory::get(std::string key, int& ttl, SAS::TrailId trail)
{
  TRC_DEBUG("SRV cache factory called for %s", key.c_str());
  std::shared_ptr<BaseResolver::SRVPriorityList> srv_list = nullptr;

  DnsResult result = _dns_client->dns_query(key, ns_t_srv, trail);

  if (!result.records().empty())
  {
    // We have a result.
    TRC_DEBUG("SRV query returned %d records", result.records().size());
    srv_list = std::make_shared<BaseResolver::SRVPriorityList>();
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

// LCOV_EXCL_START - The blacklisted function is excluded from coverage because
// it is tested in SIPResolver.

/// Return true if the host state of the given address is gray or black. Gray is
/// considered blacklisted, even if the given address is graylisted and the
/// calling code is currently probing that address.
bool BaseResolver::blacklisted(const AddrInfo& ai)
{
  pthread_mutex_lock(&_hosts_lock);

  Host::State state = host_state(ai);

  pthread_mutex_unlock(&_hosts_lock);

  return (state != Host::State::WHITE);
}
// LCOV_EXCL_STOP

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

// If no targets were resolved in either a_resolve_iter or srv_resolve_iter and
// SAS logs are being taken, this code is called
void BaseResolver::no_targets_resolved_logging(const std::string name,
                                               SAS::TrailId trail,
                                               bool whitelisted_allowed,
                                               bool blacklisted_allowed)
{
  if (whitelisted_allowed ^ blacklisted_allowed)
  {
    // The search was restricted to either just blacklisted or just
    // whitelisted addresses - there were none with the specified state.
    SAS::Event event(trail, SASEvent::BASERESOLVE_NO_ALLOWED_RECORDS, 0);
    event.add_var_param(name);
    event.add_static_param(blacklisted_allowed);
    SAS::report_event(event);
  }
  else
  {
    // No records at all for this address.
    SAS::Event event(trail, SASEvent::BASERESOLVE_NO_RECORDS, 0);
    event.add_var_param(name);
    SAS::report_event(event);
  }
}

void BaseResolver::dns_query(std::vector<std::string>& domains,
                             int dnstype,
                             std::vector<DnsResult>& results,
                             SAS::TrailId trail)
{
  _dns_client->dns_query(domains, dnstype, results, trail);
}

std::shared_ptr<SRVPriorityList> BaseResolver::get_srv_list(const std::string& srv_name,
                                                            int &ttl,
                                                            SAS::TrailId trail)
{
  return _srv_cache->get(srv_name, ttl, trail);
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

LazyAResolveIter::LazyAResolveIter(DnsResult& dns_result,
                                   BaseResolver* resolver,
                                   int port,
                                   int transport,
                                   SAS::TrailId trail,
                                   int allowed_host_state) :
  _resolver(resolver),
  _allowed_host_state(allowed_host_state),
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

std::vector<AddrInfo> LazyAResolveIter::take(int num_requested_targets)
{
  // Initialise variables for allowed host states.
  const bool whitelisted_allowed = _allowed_host_state & BaseResolver::WHITELISTED;
  const bool blacklisted_allowed = _allowed_host_state & BaseResolver::BLACKLISTED;

  // Vector of targets to be returned
  std::vector<AddrInfo> targets;

  // Strings for logging purposes.
  std::string graylisted_targets_str;
  std::string whitelisted_targets_str;
  std::string blacklisted_targets_str;
  std::string found_whitelisted_str;
  std::string found_blacklisted_str;

  // This lock must be held to safely call the host_state method of
  // BaseResolver.
  pthread_mutex_lock(&(_resolver->_hosts_lock));

  // If there are any graylisted records, and we're set to return whitelisted
  // records, the Iterator should return one first, and then no more.
  if (_first_call && whitelisted_allowed)
  {
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
        TRC_DEBUG("Added a graylisted server to targets, now have %ld of %d",
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
    std::string target = result.address_and_port_to_string() + ";";

    if (_resolver->host_state(result) == BaseResolver::Host::State::WHITE)
    {
      // Address is whitelisted. Add it to the log string of addresses found.
      // If we're allowed to return whitelisted targets, also add it to the
      // targets vector.
      found_whitelisted_str += target;

      if (whitelisted_allowed)
      {
        // Add the record to the targets list.
        targets.push_back(result);

        // Update logging.
        whitelisted_targets_str += target;
        TRC_DEBUG("Added a whitelisted server to targets, now have %ld of %d",
                  targets.size(),
                  num_requested_targets);
      }
    }
    else
    {
      // Address is blacklisted or graylisted. Add it to the log string of
      // unhealthy addresses found. If we're allowed to return blacklisted
      // targets, also add it to the unhealthy results vector, from which we'll
      // pull in extra targets if we don't have enough whitelisted targets or
      // if whitelisted targets aren't allowed.
      found_blacklisted_str += target;

      if (blacklisted_allowed)
      {
        // Add the record to the list of unhealthy targets.
        _unhealthy_results.push_back(result);

        // Update logging.
        found_blacklisted_str += target;
        TRC_DEBUG("Found an unhealthy server, now have %ld unhealthy results",
                  _unhealthy_results.size());
      }
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
    TRC_DEBUG("Added an unhealthy server to targets, now have %ld of %d",
              targets.size(),
              num_requested_targets);
  }

  if (_trail != 0)
  {
    SAS::Event event(_trail, SASEvent::BASERESOLVE_A_RESULT_TARGET_SELECT, 0);
    event.add_var_param(_hostname);
    event.add_var_param(graylisted_targets_str);
    event.add_var_param(found_whitelisted_str);
    event.add_static_param(whitelisted_allowed);
    event.add_var_param(whitelisted_targets_str);
    event.add_var_param(found_blacklisted_str);
    event.add_static_param(blacklisted_allowed);
    event.add_var_param(blacklisted_targets_str);
    SAS::report_event(event);

    if (targets.empty() && _first_call)
    {
      _resolver->no_targets_resolved_logging(_hostname,
                                             _trail,
                                             whitelisted_allowed,
                                             blacklisted_allowed);
    }
  }

  _first_call = false;

  return targets;
}

LazySRVResolveIter::LazySRVResolveIter(BaseResolver* resolver,
                                       int af,
                                       int transport,
                                       const std::string& srv_name,
                                       SAS::TrailId trail,
                                       int allowed_host_state) :
  _resolver(resolver),
  _af(af),
  _transport(transport),
  _ttl(0),
  _srv_name(srv_name),
  _trail(trail),
  _search_for_gray(true),
  _unprobed_gray_target(),
  _gray_found(false),
  _whitelisted_addresses_by_srv(),
  _unhealthy_addresses_by_srv(),
  _srvs(),
  _unhealthy_targets(),
  _current_srv(0)
{
  // Initialise variables for allowed host states.
  _whitelisted_allowed = allowed_host_state & BaseResolver::WHITELISTED;
  _blacklisted_allowed = allowed_host_state & BaseResolver::BLACKLISTED;

  // Finds and loads the relevant SRV priority list from the cache.  This
  // returns a shared pointer, so the list will not be deleted until we have
  // finished with it, but the Cache can still update the entry once the list
  // has expired.
  _srv_list = _srv_cache->get(srv_name, _ttl, trail);

  if (srv_list != nullptr)
  {
    TRC_DEBUG("Found SRV records at %ld priority levels", srv_list->size());

    // prepare_priority_level will initially look at the highest priority level.
    _priority_level_iter = _srv_list->begin();
  }
  else
  {
    TRC_DEBUG("No SRV records found");
  }
}

std::vector<AddrInfo> LazySRVResolveIter::take(int num_requested_targets)
{
  // The vector of targets to be returned.
  std::vector<AddrInfo> targets;

  // Reserve sufficient space for targets that it will never need to be
  // reallocated.
  targets.reserve(num_requested_targets);

  // Boolean to track whether unhealthy targets should be added. Is only set to
  // true if both whitelisted and blacklisted targets were requested, and every
  // whitelisted target has been returned, without reaching the number of
  // requested targets.
  bool add_unhealthy = false;

  // These strings are used for logging, to store the servers we've found, and
  // the servers that we're returning as targets.
  std::string whitelisted_targets_str;
  std::string unhealthy_targets_str;
  std::string graylisted_targets_str;

  // Tracks the number of targets left to be added.
  int num_targets_to_find = num_requested_targets;

  if (_srv_list != nullptr)
  {
    // If srv_list is null, DNS resolution found no SRVs, so an empty vector
    // should be returned.
    while (num_targets_to_find > 0)
    {
      // If this is the first request, or the previous priority level has been
      // finished, then both vectors of addresses_by_srv will be empty and
      // get_from_priority_level will do nothing and return the same number of
      // targets. Otherwise, this resumes the search of the current priority
      // level from a previous call of take.
      num_targets_to_find = get_from_priority_level(targets,
                                                    num_targets_to_find,
                                                    num_requested_targets,
                                                    whitelisted_targets_str,
                                                    unhealthy_targets_str);

      if (targets.size() > 0)
      {
        // If a graylisted target is probed by this request, it must be the
        // first target to guarantee that it is probed. It is important to
        // respect priority levels, so if targets have been found at a higher
        // priority level this request should not search for gray targets to
        // probe.
        _search_for_gray=false;
      }

      if (num_targets_to_find > 0)
      {
        // Prepares the next priority level, since more targets need to be
        // found.  If this returns false then all priority levels have already
        // been searched for targets of the desired state.
        if (!prepare_priority_level())
        {
          // All whitelisted targets have been returned but not enough targets
          // have been found, so unhealthy targets should be returned.
          add_unhealthy = true;
          break;
        }

        if (_gray_found)
        {
          // A graylisted target will only be returned if this is the first time
          // take was called for this iterator. It is guaranteed that this
          // target will be the first target returned.
          targets.push_back(_unprobed_gray_target);
          _resolver->select_for_probing(_unprobed_gray_target);
          _gray_found = false;
          --num_targets_to_find;
          TRC_DEBUG("Added a graylisted server for probing to targets, now have 1 of %d", num_requested_targets);
        }
      }
    }

    if (add_unhealthy && _whitelisted_allowed && _blacklisted_allowed)
    {
      // This adds unhealthy targets until either no unhealthy targets are left,
      // or the number of requested targets has been reached. If only whitelisted
      // or blacklisted addresses were requested then _unhealthy_targets is empty,
      // addresses of the desired host state were added straight to targets.
      size_t to_copy = std::min((size_t) num_targets_to_find, _unhealthy_targets.size());

      TRC_VERBOSE("Adding %ld unhealthy servers", to_copy);
      for (size_t ii = 0; ii < to_copy; ++ii)
      {
        targets.push_back(_unhealthy_targets[ii]);
        --num_targets_to_find;
        TRC_DEBUG("Added an unhealthy server to targets, now have %ld of %d",
                  targets.size(),
                  num_requested_targets);
        std::string unhealthy_name = "[" + _unhealthy_targets[ii].address_and_port_to_string() + "]";
        unhealthy_targets_str += unhealthy_name;
      }
    }

    if (num_targets_to_find > 0)
    {
      TRC_DEBUG("Not enough addresses found of the desired host state. Returning %d out of %ld total requested", targets.size(), num_targets_requested);
    }
  }

  if (_trail != 0)
  {
    SAS::Event event(_trail, SASEvent::BASERESOLVE_SRV_RESULT, 0);
    event.add_var_param(_srv_name);
    event.add_var_param(graylisted_targets_str);
    event.add_static_param(_whitelisted_allowed);
    event.add_var_param(whitelisted_targets_str);
    event.add_static_param(_blacklisted_allowed);
    event.add_var_param(unhealthy_targets_str);
    SAS::report_event(event);

    if (targets.empty())
    {
      _resolver->no_targets_resolved_logging(_srv_name, _trail, _whitelisted_allowed, _blacklisted_allowed);
    }
  }

  return targets;
}

int LazySRVResolveIter::get_min_ttl()
{
  return _ttl;
}

bool LazySRVResolveIter::prepare_priority_level()
{
  if (_priority_level_iter != _srv_list->end())
  {
    TRC_VERBOSE("Processing %d SRVs with priority %d", _priority_level_iter->second.size(), _priority_level_iter->first);

    // As a new priority level is being prepared, resets the position of
    // get_from_priority_level to the beginning.
    _current_srv = 0;

    // Clear the vector of SRVs for this priority level.
    _srvs.clear();
    _srvs.reserve(_priority_level_iter->second.size());

    // Build a cumulative weighted tree for this priority level.
    WeightedSelector<BaseResolver::SRV> selector(_priority_level_iter->second);

    // Select entries while there are any with non-zero weights.
    while (selector.total_weight() > 0)
    {
      int ii = selector.select();
      TRC_DEBUG("Selected SRV %s:%d, weight = %d",
                _priority_level_iter->second[ii].target.c_str(),
                _priority_level_iter->second[ii].port,
                _priority_level_iter->second[ii].weight);
      _srvs.push_back(&_priority_level_iter->second[ii]);
    }

    // Do A/AAAA record look-ups for the selected SRV targets.
    std::vector<std::string> a_targets;
    std::vector<DnsResult> a_results;

    for (size_t ii = 0; ii < _srvs.size(); ++ii)
    {
      a_targets.push_back(_srvs[ii]->target);
    }

    TRC_VERBOSE("Do A record look-ups for %ld SRVs", a_targets.size());
    _resolver->dns_query(a_targets,
                         (_af == AF_INET) ? ns_t_a : ns_t_aaaa,
                         a_results,
                         _trail);

    // Clear the two vectors where the results are stored, to store the results
    // from this priority level
    _whitelisted_addresses_by_srv.clear();
    _unhealthy_addresses_by_srv.clear();

    for (size_t ii = 0; ii < _srvs.size(); ++ii)
    {
      DnsResult& a_result = a_results[ii];
      TRC_DEBUG("SRV %s:%d returned %ld IP addresses",
                _srvs[ii]->target.c_str(),
                _srvs[ii]->port,
                a_result.records().size());
      std::vector<IP46Address> whitelisted_addresses;
      std::vector<IP46Address> unhealthy_addresses;
      whitelisted_addresses.reserve(a_result.records().size());
      unhealthy_addresses.reserve(a_result.records().size());

      // This lock must be held to safely call the host_state method of
      // BaseResolver.
      pthread_mutex_lock(&(_resolver->_hosts_lock));

      for (size_t jj = 0; jj < a_result.records().size(); ++jj)
      {
        AddrInfo ai;
        ai.transport = _transport;
        ai.port = _srvs[ii]->port;
        ai.address = _resolver->to_ip46(a_result.records()[jj]);

        BaseResolver::Host::State addr_state = _resolver->host_state(ai);
        std::string target = "[" + ai.address_and_port_to_string() + "] ";

        // If whitelisted targets are requested, the first unprobed graylisted
        // target reached will be selected for probing and put at the start of
        // the list of targets, it is treated like it is whitelisted. However,
        // if only blacklisted targets are requested then this function will not
        // search for a graylisted target to probe, all graylisted addresses
        // will be treated as though they were blacklisted.
        if (addr_state == BaseResolver::Host::State::GRAY_NOT_PROBING && _search_for_gray && _whitelisted_allowed)
        {
          // First graylisted address reached which isn't already being probed.
          // Since the graylisted address must be tested, take must add it as
          // the first target. This takes precedence over the weighting since
          // only a small proportion of requests are used for probing, so the
          // effect on request distribution is neglible.

          // Only probe one graylisted address with this request.
          _search_for_gray = false;

          // Tells take that a graylisted addresshas been found and should be
          // added to targets.
          _gray_found = true;

          // Creates the address so that take can add it as a target.
          _unprobed_gray_target.transport = _transport;
          _unprobed_gray_target.port = _srvs[ii]->port;
          _unprobed_gray_target.weight = _srvs[ii]->weight;
          _unprobed_gray_target.priority = _srvs[ii]->priority;
          _unprobed_gray_target.address = ai.address;
        }
        else if (addr_state == BaseResolver::Host::State::WHITE)
        {
          // Address is whitelisted.  If we're allowed to return whitelisted
          // targets, also add it to the whitelisted_addresses vector, from
          // which we'll select targets.
          if (_whitelisted_allowed)
          {
            whitelisted_addresses.push_back(ai.address);
          }
        }
        else
        {
          // Address is blacklisted or graylisted and not being probed by this
          // request.  If we're allowed to return blacklisted targets, also add
          // it to the unhealthy_addresses vector, from which we'll pull in
          // extra targets if we don't have enough whitelisted addresses to form
          // targets or to find our targets from if whitelisted targets aren't
          // allowed.
          if (_blacklisted_allowed)
          {
            unhealthy_addresses.push_back(ai.address);
          }
        }

        // Take the smallest ttl returned so far.
        _ttl = std::min(_ttl, a_result.ttl());
      }

      pthread_mutex_unlock(&(_resolver->_hosts_lock));

      // Randomize the order of both vectors.
      std::random_shuffle(whitelisted_addresses.begin(), whitelisted_addresses.end());
      std::random_shuffle(unhealthy_addresses.begin(), unhealthy_addresses.end());

      // Stores the whitelisted and unhealthy results for this SRV in class
      // memory so get_from_priority_level can use them.
      _whitelisted_addresses_by_srv.push_back(whitelisted_addresses);
      _unhealthy_addresses_by_srv.push_back(unhealthy_addresses);
    }

    // The next time prepare_priority_level is called it will prepare the next
    // highest priority level.
    ++_priority_level_iter;
    return true;
  }
  else
  {
    TRC_DEBUG("All priority levels have been prepared and searched for targets of the desired host state.");
    // Returns false to show that there was not a valid priority level to
    // prepare.
    return false;
  }
}

int LazySRVResolveIter::get_from_priority_level(std::vector<AddrInfo> &targets,
                                                int num_targets_to_find,
                                                int num_requested_targets,
                                                std::string& whitelisted_targets_str,
                                                std::string& unhealthy_targets_str)
{
  // Select the appropriate number of targets by looping through the SRV records
  // taking one address each time until either we have enough for the number of
  // retries allowed, or we have no more addresses.
  //
  // This takes one target from each site at this priority level and then
  // repeats, even if one site contains multiple addresses, so if the first
  // target fails the weighting will not be strictly obeyed. This is desirable
  // because if one address from a site fails an address from another site is
  // less likely to fail, and it is more important to ensure that a request
  // succeeds than that the load is well balanced.
  bool more = true;
  while ((num_targets_to_find > 0) && (more))
  {
    more = false;
    AddrInfo ai;
    ai.transport = _transport;

    // This lock must be held to safely call the host_state method of
    // BaseResolver.
    pthread_mutex_lock(&(_resolver->_hosts_lock));

    // The for loop is only initialised if on the previous iteration it went
    // through all SRVs. This lets get_from_priority_level pause the for loop if
    // it finds enough targets and resume when it is next called.
    if ((size_t) _current_srv == _srvs.size())
    {
      _current_srv = 0;
    }

    for (;
         ((size_t) _current_srv < _srvs.size()) && (num_targets_to_find > 0);
         ++_current_srv)
    {
      ai.port = _srvs[_current_srv]->port;
      ai.priority = _srvs[_current_srv]->priority;
      ai.weight = _srvs[_current_srv]->weight;

      if (!_whitelisted_addresses_by_srv[_current_srv].empty() && _whitelisted_allowed)
      {
        ai.address = _whitelisted_addresses_by_srv[_current_srv].back();
        _whitelisted_addresses_by_srv[_current_srv].pop_back();

        if (_resolver->host_state(ai) == BaseResolver::Host::State::WHITE)
        {
          // Verifies that the address is still whitelisted. It may have been a
          // while since the priority level was prepared, so it is possible that
          // Clearwater since moved the address off of the whitelist.
          targets.push_back(ai);
          --num_targets_to_find;
          std::string target = "[" + ai.address_and_port_to_string() + "] ";
          whitelisted_targets_str += target;
          TRC_DEBUG("Added a whitelisted server to targets, now have %ld of %d",
                    targets.size(),
                    num_requested_targets);
        }
        else
        {
          _unhealthy_addresses_by_srv[_current_srv].push_back(ai.address);
          TRC_DEBUG("%s has moved from the whitelist to the blacklist since priority level %d was prepared",
                    ai.to_string().c_str(),
                    _priority_level_iter->first);
        }
      }
      else if (!_whitelisted_allowed && _blacklisted_allowed &&
               !_unhealthy_addresses_by_srv[_current_srv].empty())
      {
        // If only blacklisted targets were requested then unhealthy addresses
        // should be added to targets instead of whitelisted ones.
        ai.address = _unhealthy_addresses_by_srv[_current_srv].back();
        _unhealthy_addresses_by_srv[_current_srv].pop_back();
        if (_resolver->host_state(ai) != BaseResolver::Host::State::WHITE)
        {
          // Verifies that the address is still not whitelisted. It may have been a
          // while since the priority level was prepared, so it is possible that
          // Clearwater since moved the address to the whitelist.
          targets.push_back(ai);
          --num_targets_to_find;
          std::string target = "[" + ai.address_and_port_to_string() + "] ";
          unhealthy_targets_str += target;

          // num_requested_targets was passed to this function solely for logging
          // purposes.
          TRC_DEBUG("Only blacklisted targets were requested, so added a blacklisted server to targets, now have %ld of %d",
                    targets.size(),
                    num_requested_targets);
        }
        else
        {
          _whitelisted_addresses_by_srv[_current_srv].push_back(ai.address);
          TRC_DEBUG("%s has moved from the blacklist or graylist to the whitelist since priority level %d was prepared",
                    ai.to_string().c_str(),
                    _priority_level_iter->first);
        }
      }

      if (!_unhealthy_addresses_by_srv[_current_srv].empty() && _blacklisted_allowed && _whitelisted_allowed)
      {
        // If all host states for addresses were requested, a vector of
        // unhealthy targets is compiled, to be used for additional targets if
        // there are insufficiently many whitelisted addresses over all priority
        // levels for take to return.
        ai.address = _unhealthy_addresses_by_srv[_current_srv].back();
        _unhealthy_addresses_by_srv[_current_srv].pop_back();
        _unhealthy_targets.push_back(ai);
      }

      more = more ||
             ((!_whitelisted_addresses_by_srv[_current_srv].empty()) ||
             (!_unhealthy_addresses_by_srv[_current_srv].empty()));
    }

    // host_state no longer needs to be called, so this lock can be relaxed.
    pthread_mutex_unlock(&(_resolver->_hosts_lock));
  }
  return num_targets_to_find;
}
