/**
 * @file baseresolver.cpp  Implementation of base class for DNS resolution.
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

#include <time.h>

#include <algorithm>
#include <sstream>
#include <iomanip>

#include "log.h"
#include "utils.h"
#include "baseresolver.h"
#include "sas.h"
#include "sasevent.h"

BaseResolver::BaseResolver(DnsCachedResolver* dns_client) :
  _naptr_factory(),
  _naptr_cache(),
  _srv_factory(),
  _srv_cache(),
  _blacklist(),
  _dns_client(dns_client)
{
}

BaseResolver::~BaseResolver()
{
}

// Removes all the entries from the blacklist.
void BaseResolver::clear_blacklist()
{
  pthread_mutex_lock(&_blacklist_lock);
  _blacklist.clear();
  pthread_mutex_unlock(&_blacklist_lock);
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
void BaseResolver::create_blacklist(int blacklist_duration)
{
  // Create the blacklist (no factory required).
  TRC_DEBUG("Create black list");
  pthread_mutex_init(&_blacklist_lock, NULL);
  _default_blacklist_duration = blacklist_duration;
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
  pthread_mutex_destroy(&_blacklist_lock);
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
      SRVWeightedSelector selector(i->second);

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
        }

        // Take the smallest ttl returned so far.
        ttl = std::min(ttl, a_result.ttl());

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
            char buf[100];
            std::string target = inet_ntop(ai.address.af,
                                           &ai.address.addr,
                                           buf, sizeof(buf));
            std::string tg = "[" + target + ":" + std::to_string(ai.port) + "] ";
            targetlist_str = targetlist_str + tg;

            TRC_VERBOSE("Added a server, now have %ld of %d", targets.size(), retries);
          }

          if (!blacklist_addr[ii].empty())
          {
            ai.address = blacklist_addr[ii].back();
            blacklist_addr[ii].pop_back();
            blacklisted_targets.push_back(ai);
            char buf[100];
            std::string blacklistee = inet_ntop(ai.address.af,
                                                &ai.address.addr,
                                                buf, sizeof(buf));
            std::string bl = "[" + blacklistee + ":" + std::to_string(ai.port) + "] ";
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
        char buf[100];
        std::string blacklistee = inet_ntop(blacklisted_targets[ii].address.af,
                                            &blacklisted_targets[ii].address.addr,
                                            buf, sizeof(buf));
        std::string bl = "[" + blacklistee + ":" + std::to_string(blacklisted_targets[ii].port) + "]";
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
  // Clear the list of targets just in case.
  targets.clear();

  // Accumulate blacklisted targets in case they are needed.
  std::vector<AddrInfo> blacklisted_targets;

  // Do A/AAAA lookup.
  DnsResult result = _dns_client->dns_query(hostname, (af == AF_INET) ? ns_t_a : ns_t_aaaa, trail);
  ttl = result.ttl();

  // Randomize the records in the result.
  TRC_DEBUG("Found %ld A/AAAA records, randomizing", result.records().size());
  std::random_shuffle(result.records().begin(), result.records().end());

  // Loop through the records in the result picking non-blacklisted targets.
  AddrInfo ai;
  ai.transport = transport;
  ai.port = port;
  std::string targetlist_str;
  std::string blacklist_str;
  std::string added_from_blacklist_str;

  for (std::vector<DnsRRecord*>::const_iterator i = result.records().begin();
       i != result.records().end();
       ++i)
  {
    ai.address = to_ip46(*i);
    if (!blacklisted(ai))
    {
      // Address isn't blacklisted, so copy across to the target list.
      targets.push_back(ai);
      targetlist_str = targetlist_str + (*i)->to_string() + ";";
      TRC_DEBUG("Added a server, now have %ld of %d", targets.size(), retries);
    }
    else
    {
      // Address is blacklisted, so copy to blacklisted list.
      blacklisted_targets.push_back(ai);
      blacklist_str = blacklist_str + (*i)->to_string() + ";";
    }

    if (targets.size() >= (size_t)retries)
    {
      // We have enough targets so stop looking at records.
      TRC_DEBUG("Have enough targets");

      if (trail != 0)
      {
        SAS::Event event(trail, SASEvent::BASERESOLVE_A_RESULT, 0);
        event.add_var_param(hostname);
        event.add_var_param(targetlist_str);
        event.add_var_param(blacklist_str);
        event.add_var_param(added_from_blacklist_str);
        SAS::report_event(event);
      }

      break;
    }
  }

  // If we've gone through the whole set of A/AAAA record and haven't found
  // enough unblacklisted targets, add blacklisted targets.
  if (targets.size() < (size_t)retries)
  {
    size_t to_copy = (size_t)retries - targets.size();
    if (to_copy > blacklisted_targets.size())
    {
      to_copy = blacklisted_targets.size();
    }

    TRC_DEBUG("Adding %ld servers from blacklist", to_copy);

    for (size_t ii = 0; ii < to_copy; ++ii)
    {
      targets.push_back(blacklisted_targets[ii]);
      char buf[100];
      std::string blacklistee = inet_ntop(blacklisted_targets[ii].address.af,
                                          &blacklisted_targets[ii].address.addr,
                                          buf, sizeof(buf));
      std::string bl = "[" + blacklistee + ":" + std::to_string(blacklisted_targets[ii].port) + "]";
      added_from_blacklist_str = added_from_blacklist_str + bl;
    }
  }

  if (trail != 0)
  {
    SAS::Event event(trail, SASEvent::BASERESOLVE_A_RESULT, 0);
    event.add_var_param(hostname);
    event.add_var_param(targetlist_str);
    event.add_var_param(blacklist_str);
    event.add_var_param(added_from_blacklist_str);
    SAS::report_event(event);
  }
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
void BaseResolver::blacklist(const AddrInfo& ai, int ttl)
{
  char buf[100];
  TRC_DEBUG("Add %s:%d transport %d to blacklist for %d seconds",
            inet_ntop(ai.address.af, &ai.address.addr, buf, sizeof(buf)),
            ai.port, ai.transport, ttl);
  pthread_mutex_lock(&_blacklist_lock);
  _blacklist[ai] = time(NULL) + ttl;
  pthread_mutex_unlock(&_blacklist_lock);
}

bool BaseResolver::blacklisted(const AddrInfo& ai)
{
  bool rc = false;

  pthread_mutex_lock(&_blacklist_lock);
  Blacklist::iterator i = _blacklist.find(ai);

  if (i != _blacklist.end())
  {
    if (i->second > time(NULL))
    {
      // Blacklist entry has yet to expire.
      rc = true;
    }
    else
    {
      // Blacklist entry has expired, so remove it.
      _blacklist.erase(i);
    }
  }
  pthread_mutex_unlock(&_blacklist_lock);
  char buf[100];
  TRC_DEBUG("%s:%d transport %d is %sblacklisted",
            inet_ntop(ai.address.af, &ai.address.addr, buf, sizeof(buf)),
            ai.port, ai.transport, rc ? "" : "not ");

  return rc;
}

/// Parses a target as if it was an IPv4 or IPv6 address and returns the
/// status of the parse.
bool BaseResolver::parse_ip_target(const std::string& target, IP46Address& address)
{
  // Assume the parse fails.
  TRC_DEBUG("Attempt to parse %s as IP address", target.c_str());
  bool rc = false;

  // Strip start and end white-space.
  std::string ip_target = target;
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


BaseResolver::SRVWeightedSelector::SRVWeightedSelector(const std::vector<SRV>& srvs) :
  _tree(srvs.size())
{
  // Copy the weights to the tree.
  for (size_t ii = 0; ii < srvs.size(); ++ii)
  {
    _tree[ii] = srvs[ii].weight;
  }

  // Work backwards up the tree accumulating the weights.
  for (size_t ii = _tree.size() - 1; ii >= 1; --ii)
  {
    _tree[(ii - 1)/2] += _tree[ii];
  }
}

BaseResolver::SRVWeightedSelector::~SRVWeightedSelector()
{
}

int BaseResolver::SRVWeightedSelector::select()
{
  // Search the tree to find the item with the smallest cumulative weight that
  // is greater than a random number between zero and the total weight of the
  // tree.
  int s = rand() % _tree[0];
  size_t ii = 0;

  while (true)
  {
    // Find the left and right children using the usual tree => array mappings.
    size_t l = 2*ii + 1;
    size_t r = 2*ii + 2;

    if ((l < _tree.size()) && (s < _tree[l]))
    {
      // Selection is somewhere in left subtree.
      ii = l;
    }
    else if ((r < _tree.size()) && (s >= _tree[ii] - _tree[r]))
    {
      // Selection is somewhere in right subtree.
      s -= (_tree[ii] - _tree[r]);
      ii = r;
    }
    else
    {
      // Found the selection.
      break;
    }
  }

  // Calculate the weight of the selected entry by subtracting the weight of
  // its left and right subtrees.
  int weight = _tree[ii] -
               (((2*ii + 1) < _tree.size()) ? _tree[2*ii + 1] : 0) -
               (((2*ii + 2) < _tree.size()) ? _tree[2*ii + 2] : 0);

  // Update the tree to set the weight of the selection to zero so it isn't
  // selected again.
  _tree[ii] -= weight;
  int p = ii;
  while (p > 0)
  {
    p = (p - 1)/2;
    _tree[p] -= weight;
  }

  return ii;
}

int BaseResolver::SRVWeightedSelector::total_weight()
{
  return _tree[0];
}
