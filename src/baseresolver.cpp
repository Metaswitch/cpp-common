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

void BaseResolver::add_target_to_log_string(std::string& log_string,
                                            const AddrInfo& addr,
                                            const std::string& state)
{
  if (!log_string.empty())
  {
    log_string += ", ";
  }

  log_string += addr.address_and_port_to_string() + " (" + state + ")";
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
  return new LazySRVResolveIter(this,
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

std::shared_ptr<BaseResolver::NAPTRReplacement> BaseResolver::NAPTRCacheFactory::get(std::string key,
                                                                                     int& ttl,
                                                                                     SAS::TrailId trail)
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

std::shared_ptr<BaseResolver::SRVPriorityList> BaseResolver::SRVCacheFactory::get(std::string key,
                                                                                  int& ttl,
                                                                                  SAS::TrailId trail)
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

BaseResolver::Host::Host(int blacklist_ttl, int graylist_ttl) :
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

BaseResolver::Host::State BaseResolver::host_state(const AddrInfo& ai,
                                                   time_t current_time)
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

bool BaseResolver::select_address(const AddrInfo& addr,
                                  SAS::TrailId trail,
                                  int allowed_host_state)
{
  bool allowed;
  const bool whitelisted_allowed = allowed_host_state & BaseResolver::WHITELISTED;
  const bool blacklisted_allowed = allowed_host_state & BaseResolver::BLACKLISTED;

  pthread_mutex_lock(&_hosts_lock);

  BaseResolver::Host::State state = host_state(addr);

  switch (state)
  {
  case BaseResolver::Host::State::WHITE:
    allowed = whitelisted_allowed;
    break;

  case BaseResolver::Host::State::GRAY_NOT_PROBING:
    allowed = whitelisted_allowed;

    // If the address is allowed, we need to mark it as being probed (so that
    // further requests do not consider it to whitelisted).
    if (allowed)
    {
      select_for_probing(addr);
    }
    break;

  case BaseResolver::Host::State::GRAY_PROBING:
    allowed = blacklisted_allowed;
    break;

  case BaseResolver::Host::State::BLACK:
    allowed = blacklisted_allowed;
    break;

  default:
    // LCOV_EXCL_START
    TRC_WARNING("Unknown host state %d", (int)state);
    allowed = false;
    break;
    // LCOV_EXCL_STOP
  }

  pthread_mutex_unlock(&_hosts_lock);

  std::string host_state_str = BaseResolver::Host::state_to_string(state);
  std::string addr_str = addr.address_and_port_to_string();

  TRC_DEBUG("Address %s is in state %s and %s allowed to be used based on an "
            "allowed host state bitfield of 0x%x",
            addr_str.c_str(),
            host_state_str.c_str(),
            allowed ? "is" : "is not",
            allowed_host_state);

  if (allowed)
  {
    SAS::Event event(trail, SASEvent::BASERESOLVE_IP_ALLOWED, 0);
    event.add_var_param(addr_str);
  }
  else
  {
    SAS::Event event(trail, SASEvent::BASERESOLVE_IP_NOT_ALLOWED, 0);
    event.add_static_param(whitelisted_allowed);
    event.add_static_param(blacklisted_allowed);
    event.add_var_param(addr_str);
    event.add_var_param(host_state_str);
  }

  return allowed;
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

// If no targets were resolved in either a_resolve_iter or srv_resolve_iter and
// SAS logs are being taken, this code is called
void BaseResolver::no_targets_resolved_logging(const std::string name,
                                               SAS::TrailId trail,
                                               bool whitelisted_allowed,
                                               bool blacklisted_allowed)
{
  if (whitelisted_allowed != blacklisted_allowed)
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

std::shared_ptr<BaseResolver::SRVPriorityList> BaseResolver::get_srv_list(const std::string& srv_name,
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
  TRC_DEBUG("Attempting to get %d targets for host:%s. allowed_host_state = %d",
            num_requested_targets,
            _hostname.c_str(),
            _allowed_host_state);

  // Initialise variables for allowed host states.
  const bool whitelisted_allowed = _allowed_host_state & BaseResolver::WHITELISTED;
  const bool blacklisted_allowed = _allowed_host_state & BaseResolver::BLACKLISTED;

  // Vector of targets to be returned
  std::vector<AddrInfo> targets;
  std::string targets_log_str;

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
        BaseResolver::add_target_to_log_string(targets_log_str, *result_it, "graylisted");
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
  // required number of targets is reached, or the unused results are exhausted.
  // If only blacklisted targets are allowed to be returned, instead add
  // unhealthy results to targets and add nothing to the unhealthy results
  // vector.
  while ((_unused_results.size() > 0) &&
         (targets.size() < (size_t)num_requested_targets))
  {
    AddrInfo result = _unused_results.back();
    _unused_results.pop_back();
    std::string target = result.address_and_port_to_string() + ";";

    if (_resolver->host_state(result) == BaseResolver::Host::State::WHITE)
    {
      // Address is whitelisted. If we're allowed to return whitelisted
      // targets, also add it to the targets vector.

      if (whitelisted_allowed)
      {
        // Add the record to the targets list.
        targets.push_back(result);

        // Update logging.
        BaseResolver::add_target_to_log_string(targets_log_str, result, "whitelisted");
        TRC_DEBUG("Added a whitelisted server to targets, now have %ld of %d",
                  targets.size(),
                  num_requested_targets);
      }
    }
    else
    {
      // Address is blacklisted or graylisted. If we're allowed to return
      // blacklisted targets, also add it to the unhealthy results vector, from
      // which we'll pull in extra targets if we don't have enough whitelisted
      // targets or if whitelisted targets aren't allowed.

      if (blacklisted_allowed)
      {
        if (whitelisted_allowed)
        {
          // Add the record to the list of unhealthy targets.
          _unhealthy_results.push_back(result);

          // Update logging.
          TRC_DEBUG("Found an unhealthy server, now have %ld unhealthy results",
                    _unhealthy_results.size());
        }
        else
        {
          // Only blacklisted targets are requested, so add these unhealthy
          // addresses straight to the vector of targets. Graylisted addresses
          // are considered blacklisted in this case.
          targets.push_back(result);
          TRC_DEBUG("Added a blacklisted or graylisted server to targets, now have %ld of %d",
                    targets.size(),
                    num_requested_targets);
        }
      }
    }
  }

  pthread_mutex_unlock(&(_resolver->_hosts_lock));

  // If the targets vector does not yet contain enough targets, add unhealthy
  // targets. If only whitelisted or only blacklisted targets were requested,
  // the unhealthy results vector is empty.
  while ((_unhealthy_results.size() > 0) && (targets.size() < (size_t)num_requested_targets))
  {
    AddrInfo result = _unhealthy_results.back();
    _unhealthy_results.pop_back();

    // Add the record to the targets list
    targets.push_back(result);

    // Update logging.
    BaseResolver::add_target_to_log_string(targets_log_str, result, "unhealthy");
    TRC_DEBUG("Added an unhealthy server to targets, now have %ld of %d",
              targets.size(),
              num_requested_targets);
  }

  if (_trail != 0)
  {
    SAS::Event event(_trail, SASEvent::BASERESOLVE_A_RESULT_TARGET_SELECT, 0);
    event.add_static_param(whitelisted_allowed);
    event.add_static_param(blacklisted_allowed);
    event.add_var_param(_hostname);
    event.add_var_param(targets_log_str);
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
  _srv_name(srv_name),
  _ttl(0),
  _trail(trail),
  _search_for_gray(true),
  _unprobed_gray_target(),
  _gray_found(false),
  _whitelisted_addresses_by_srv(),
  _unhealthy_addresses_by_srv(),
  _unhealthy_targets(),
  _current_srv(0),
  _unhealthy_target_pos(0)
{
  // Initialise variables for allowed host states.
  _whitelisted_allowed = allowed_host_state & BaseResolver::WHITELISTED;
  _blacklisted_allowed = allowed_host_state & BaseResolver::BLACKLISTED;

  // Finds and loads the relevant SRV priority list from the cache.  This
  // returns a shared pointer, so the list will not be deleted until we have
  // finished with it, but the Cache can still update the entry once the list
  // has expired.
  _srv_list = _resolver->get_srv_list(srv_name, _ttl, trail);

  if (_srv_list != nullptr)
  {
    TRC_DEBUG("Found SRV records at %ld priority levels", _srv_list->size());

    // prepare_priority_level will initially look at the highest priority level.
    _next_priority_level = _srv_list->begin();
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

  // String used for SAS logging the targets we've returned.
  std::string targets_log_str;

  // Tracks the number of targets left to be added.
  int num_targets_to_find = num_requested_targets;

  // Checks whether a list of SRV Records was found by DNS Resolution. If it
  // wasn't, an empty vector will be returned.
  if (_srv_list != nullptr)
  {
    while (num_targets_to_find > 0)
    {
      // If get_from_priority_level finished searching the previous priority
      // level, or no priority level has yet been prepared, prepare the next
      // one.
      if (priority_level_complete())
      {
        // Prepares the next priority level. If this returns false then all
        // priority levels have already been searched for targets of the desired
        // state.
        if (!prepare_priority_level())
        {
          // If both whitelisted and blacklisted targets were requested, all
          // whitelisted targets have been returned but not enough targets
          // have been found, so unhealthy targets should be returned.
          add_unhealthy = _whitelisted_allowed && _blacklisted_allowed;
          break;
        }
      }

      // If this is the first request, or the previous priority level has been
      // finished, then both vectors of *_addresses_by_srv will be empty and
      // get_from_priority_level will do nothing and return the same number of
      // targets. Otherwise, this resumes the search of the current priority
      // level from a previous call of take.
      num_targets_to_find = get_from_priority_level(targets,
                                                    num_targets_to_find,
                                                    num_requested_targets,
                                                    targets_log_str);
    }

    if (add_unhealthy)
    {
      // This adds unhealthy targets until either no unhealthy targets are left,
      // or the number of requested targets has been reached. If only whitelisted
      // or blacklisted addresses were requested then _unhealthy_targets is empty,
      // addresses of the desired host state were added straight to targets.
      size_t to_copy = std::min((size_t) num_targets_to_find, _unhealthy_targets.size() - _unhealthy_target_pos);
      TRC_VERBOSE("Adding %ld unhealthy servers", to_copy);

      // The for loop is initialised in the class constructor, as the for loop
      // needs to be paused and resumed in subsequent calls to take, so that the
      // same unhealthy targets are not returned several times. The for loop
      // increments _unhealthy_targets_pos each time, so ii is used solely to
      // ensure that the for loop runs for the correct number of iterations.
      for (size_t ii = 0; ii < to_copy; ++ii)
      {
        // A class data member tracks the position in the unhealthy target
        // vector, so that in subsequent calls to take the same unhealthy
        // targets are not returned twice.
        targets.push_back(_unhealthy_targets[_unhealthy_target_pos]);

        BaseResolver::add_target_to_log_string(targets_log_str,
                                               _unhealthy_targets[_unhealthy_target_pos],
                                               "unhealthy");
        TRC_DEBUG("Added an unhealthy server to targets, now have %ld of %d",
                  targets.size(),
                  num_requested_targets);

        --num_targets_to_find;
        ++_unhealthy_target_pos;
      }
    }

    if (num_targets_to_find > 0)
    {
      TRC_DEBUG("Not enough addresses found of the desired host state. Returning %d out of %ld total requested", targets.size(), num_requested_targets);
    }
  }

  if (_trail != 0)
  {
    SAS::Event event(_trail, SASEvent::BASERESOLVE_SRV_RESULT, 0);
    event.add_static_param(_whitelisted_allowed);
    event.add_static_param(_blacklisted_allowed);
    event.add_var_param(_srv_name);
    event.add_var_param(targets_log_str);
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
  if (_next_priority_level != _srv_list->end())
  {
    TRC_VERBOSE("Processing %d SRVs with priority %d", _next_priority_level->second.size(), _next_priority_level->first);

    // Clear the data member vectors that need to be reused for the new priority
    // level
    _whitelisted_addresses_by_srv.clear();
    _unhealthy_addresses_by_srv.clear();

    // As a new priority level is being prepared, tell get_from_priority_level
    // to start searching the priority level from the first SRV.
    _current_srv = 0;

    std::vector<const BaseResolver::SRV*> srvs;
    srvs.reserve(_next_priority_level->second.size());

    // Build a cumulative weighted tree for this priority level. This will use
    // the weights of the SRVs in this priority level to put them in a random
    // permutation, where an SRV is more likely to be close to the front if it
    // has a higher weight. This is used for load balancing purposes, as a
    // request will be sent to the first SRV if possible.
    WeightedSelector<BaseResolver::SRV> selector(_next_priority_level->second);

    // Select entries while there are any with non-zero weights.
    while (selector.total_weight() > 0)
    {
      int ii = selector.select();
      TRC_DEBUG("Selected SRV %s:%d, weight = %d",
                _next_priority_level->second[ii].target.c_str(),
                _next_priority_level->second[ii].port,
                _next_priority_level->second[ii].weight);
      srvs.push_back(&_next_priority_level->second[ii]);
    }

    // Do A/AAAA record look-ups for the selected SRV targets.
    std::vector<std::string> a_targets;
    std::vector<DnsResult> a_results;
    a_targets.reserve(srvs.size());
    a_results.reserve(srvs.size());

    for (size_t ii = 0; ii < srvs.size(); ++ii)
    {
      a_targets.push_back(srvs[ii]->target);
    }

    TRC_VERBOSE("Do A record look-ups for %ld SRVs", a_targets.size());
    _resolver->dns_query(a_targets,
                         (_af == AF_INET) ? ns_t_a : ns_t_aaaa,
                         a_results,
                         _trail);

    // Give each 2D vector an empty vector corresponding to each SRV record.
    _whitelisted_addresses_by_srv.resize(srvs.size());
    _unhealthy_addresses_by_srv.resize(srvs.size());

    for (size_t ii = 0; ii < srvs.size(); ++ii)
    {
      DnsResult& a_result = a_results[ii];
      TRC_DEBUG("SRV %s:%d returned %ld IP addresses",
                srvs[ii]->target.c_str(),
                srvs[ii]->port,
                a_result.records().size());
      std::vector<AddrInfo> &whitelisted_addresses = _whitelisted_addresses_by_srv[ii];
      std::vector<AddrInfo> &unhealthy_addresses = _unhealthy_addresses_by_srv[ii];

      // This lock must be held to safely call the host_state method of
      // BaseResolver.
      pthread_mutex_lock(&(_resolver->_hosts_lock));

      // Creates a template address info object. Each iteration of the for loop
      // adds a copy with the correct address to a *_addresses vector.
      AddrInfo ai;
      ai.transport = _transport;
      ai.port = srvs[ii]->port;
      ai.weight = srvs[ii]->weight;
      ai.priority = srvs[ii]->priority;

      for (size_t jj = 0; jj < a_result.records().size(); ++jj)
      {
        ai.address = _resolver->to_ip46(a_result.records()[jj]);

        BaseResolver::Host::State addr_state = _resolver->host_state(ai);
        std::string target = "[" + ai.address_and_port_to_string() + "] ";

        // If whitelisted targets are requested, the first unprobed graylisted
        // target reached will be selected for probing and put at the start of
        // the list of targets, it is treated like it is whitelisted. However,
        // if only blacklisted targets are requested then this function will not
        // search for a graylisted target to probe, all graylisted addresses
        // will be treated as though they were blacklisted.
        if ((addr_state == BaseResolver::Host::State::GRAY_NOT_PROBING) &&
            _search_for_gray &&
            _whitelisted_allowed)
        {
          // First graylisted address reached which isn't already being probed.
          // Since the graylisted address must be tested, take must add it as
          // the first target. This takes precedence over the weighting since
          // only a small proportion of requests are used for probing, so the
          // effect on request distribution is neglible.

          // Only probe one graylisted address with this request.
          _search_for_gray = false;

          // Tells take that a graylisted address has been found and should be
          // added to targets.
          _gray_found = true;

          // Stores the address in _unprobed_gray_target, so
          // get_from_priority_level can access it.
          _unprobed_gray_target = ai;
        }
        else if (addr_state == BaseResolver::Host::State::WHITE)
        {
          // Address is whitelisted.  If we're allowed to return whitelisted
          // targets, also add it to the whitelisted_addresses vector, from
          // which we'll select targets.
          if (_whitelisted_allowed)
          {
            whitelisted_addresses.push_back(ai);
          }
        }
        else
        {
          // Address is blacklisted or graylisted and not being probed by this
          // request.  If we're allowed to return blacklisted targets, add it to
          // the unhealthy_addresses vector, from which we'll pull in extra
          // targets if we don't have enough whitelisted addresses to form
          // targets or to find our targets from if whitelisted targets aren't
          // allowed.
          if (_blacklisted_allowed)
          {
            unhealthy_addresses.push_back(ai);
          }
        }

        // Take the smallest ttl returned so far.
        _ttl = std::min(_ttl, a_result.ttl());
      }

      pthread_mutex_unlock(&(_resolver->_hosts_lock));

      // Randomize the order of both vectors.
      std::random_shuffle(whitelisted_addresses.begin(), whitelisted_addresses.end());
      std::random_shuffle(unhealthy_addresses.begin(), unhealthy_addresses.end());
    }

    // The next time prepare_priority_level is called it will prepare the next
    // highest priority level.
    ++_next_priority_level;
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
                                                const int num_requested_targets,
                                                std::string& targets_log_str)
{
  // An AddrInfo object to temporarily store the current AddrInfo object being
  // looked at from a *_addresses_by_srv vector.
  AddrInfo ai;

  if (_gray_found && (num_targets_to_find > 0))
  {
    // prepare_priority_level found a graylisted target to probe, so return it.
    // It is guaranteed that this target will be the first ever returned by the
    // iterator, since prepare_priority_level will only search for a graylisted
    // target to probe at the highest priority level, or if no targets at higher
    // priority levels were returned.
    targets.push_back(_unprobed_gray_target);
    BaseResolver::add_target_to_log_string(targets_log_str,
                                           _unprobed_gray_target,
                                           "graylisted");

    _resolver->select_for_probing(_unprobed_gray_target);
    _gray_found = false;
    --num_targets_to_find;
    TRC_DEBUG("Added a graylisted server for probing to targets, now have 1 of %d", num_requested_targets);
  }

  // Select the appropriate number of targets by looping through the SRV records
  // taking one address each time until either we have enough for the number of
  // retries allowed, or we have no more addresses.
  //
  // This takes one target from each SRV Record at this priority level and then
  // goes through the SRVs again in the same order, etc, so if the first target
  // fails the weighting will not be strictly obeyed. This is desirable because
  // if one address from a SRV Record fails an address from another SRV Record
  // is less likely to fail than one from the same SRV Record, and it is more
  // important to ensure that a request succeeds than that the load is well
  // balanced.
  //
  // If this function gets the required number of targets before looking at
  // every address in the priority level, the algorithm will pause and resume
  // next time this function is called.
  while ((num_targets_to_find > 0) && (!priority_level_complete()))
  {
    // This lock must be held to safely call the host_state method of
    // BaseResolver.
    pthread_mutex_lock(&(_resolver->_hosts_lock));

    // If we're at the end of the SRV Records, start from the beginning. This
    // lets get_from_priority_level pause the for loop if it finds enough
    // targets before reaching the end of the SRV Records and resume when it is
    // next called.
    if ((size_t) _current_srv == _whitelisted_addresses_by_srv.size())
    {
      _current_srv = 0;
    }

    for (;
         ((size_t)_current_srv < _whitelisted_addresses_by_srv.size()) && (num_targets_to_find > 0);
         ++_current_srv)
    {
      std::vector<AddrInfo> &whitelisted_addresses = _whitelisted_addresses_by_srv[_current_srv];
      std::vector<AddrInfo> &unhealthy_addresses = _unhealthy_addresses_by_srv[_current_srv];

      if (!whitelisted_addresses.empty() && _whitelisted_allowed)
      {
        ai = whitelisted_addresses.back();
        whitelisted_addresses.pop_back();

        if (_resolver->host_state(ai) == BaseResolver::Host::State::WHITE)
        {
          // Verifies that the address is still whitelisted. It may have been a
          // while since the priority level was prepared, so it is possible that
          // Clearwater since moved the address off of the whitelist.
          targets.push_back(ai);
          --num_targets_to_find;

          BaseResolver::add_target_to_log_string(targets_log_str, ai, "whitelisted");
          TRC_DEBUG("Added a whitelisted server to targets, now have %ld of %d",
                    targets.size(),
                    num_requested_targets);
        }
        else if (_blacklisted_allowed)
        {
          unhealthy_addresses.push_back(ai);
          TRC_DEBUG("%s has moved from the whitelist to the blacklist since the current priority level",
                    ai.to_string().c_str());
        }
      }
      else if (!_whitelisted_allowed && _blacklisted_allowed &&
               !unhealthy_addresses.empty())
      {
        // If only blacklisted targets were requested then unhealthy addresses
        // should be added to targets instead of whitelisted ones.
        ai = unhealthy_addresses.back();
        unhealthy_addresses.pop_back();
        if (_resolver->host_state(ai) != BaseResolver::Host::State::WHITE)
        {
          // Verifies that the address is still not whitelisted. It may have been a
          // while since the priority level was prepared, so it is possible that
          // Clearwater since moved the address to the whitelist.
          targets.push_back(ai);
          --num_targets_to_find;

          BaseResolver::add_target_to_log_string(targets_log_str, ai, "unhealthy");
          TRC_DEBUG("Only blacklisted targets were requested, so added a blacklisted server to targets, now have %ld of %d",
                    targets.size(),
                    num_requested_targets);
        }
        else
        {
          TRC_DEBUG("%s has moved from the blacklist or graylist to the whitelist since the current priority level was prepared",
                    ai.to_string().c_str());
        }
      }

      if (!unhealthy_addresses.empty() && _blacklisted_allowed && _whitelisted_allowed)
      {
        // If all host states for addresses were requested, a vector of
        // unhealthy targets is compiled, to be used for additional targets if
        // there are insufficiently many whitelisted addresses over all priority
        // levels for take to return.
        ai = unhealthy_addresses.back();
        unhealthy_addresses.pop_back();
        _unhealthy_targets.push_back(ai);
      }
    }

    // host_state no longer needs to be called, so this lock can be
    // released.
    pthread_mutex_unlock(&(_resolver->_hosts_lock));
  }

  if (targets.size() > 0)
  {
    // If a graylisted target is probed by this request, it must be the
    // first target to guarantee that it is probed. It is important to
    // respect priority levels, so if targets have been found at a higher
    // priority level this request should not search for gray targets to
    // probe.
    _search_for_gray=false;
  }

  return num_targets_to_find;
}

/// Returns true if every SRV record in the priority level last prepared has no
/// whitelisted or unhealthy addresses left that get_from_priority_level hasn't
/// looked at, or if no priority level has been prepared yet.
bool LazySRVResolveIter::priority_level_complete()
{
  bool complete = true;

  // Scan through each SRV record at the current priority level, and if any of
  // them still contain an address, return false. If no priority level was
  // prepared yet, _whitelisted_addresses_by_srv will be empty. Note that
  // _whitelisted_addresses_by_srv and _unhealthy_addresses_by_src will be the
  // same size, as they have one entry per SRV at the same priority level.
  for (size_t ii = 0; ii < _whitelisted_addresses_by_srv.size(); ++ii)
  {
    if (!_whitelisted_addresses_by_srv[ii].empty() ||
        !_unhealthy_addresses_by_srv[ii].empty())
    {
      // At least one address is left in this SRV, so the level is not
      // complete.
      complete = false;
    }
  }

  return complete;
}
