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

#include "log.h"
#include "utils.h"
#include "baseresolver.h"


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

// Creates the cache for storing NAPTR results.
void BaseResolver::create_naptr_cache(const std::map<std::string, int> naptr_services)
{
  // Create the NAPTR cache factory and the cache itself.
  LOG_DEBUG("Create NAPTR cache");
  _naptr_factory = new NAPTRCacheFactory(naptr_services, DEFAULT_TTL, _dns_client);
  _naptr_cache = new NAPTRCache(_naptr_factory);
}

/// Creates the cache for storing SRV results and selectors.
void BaseResolver::create_srv_cache()
{
  // Create the factory and cache for SRV.
  LOG_DEBUG("Create SRV cache");
  _srv_factory = new SRVCacheFactory(DEFAULT_TTL, _dns_client);
  _srv_cache = new SRVCache(_srv_factory);
}

/// Creates the blacklist of address/port/transport triplets.
void BaseResolver::create_blacklist()
{
  // Create the blacklist (no factory required).
  LOG_DEBUG("Create black list");
  _blacklist = new BlacklistCache(NULL);
}

void BaseResolver::destroy_naptr_cache()
{
  LOG_DEBUG("Destroy NAPTR cache");
  delete _naptr_cache;
  delete _naptr_factory;
}

void BaseResolver::destroy_srv_cache()
{
  LOG_DEBUG("Destroy SRV cache");
  delete _srv_cache;
  delete _srv_factory;
}

void BaseResolver::destroy_blacklist()
{
  LOG_DEBUG("Destroy blacklist");
  delete _blacklist;
}

/// Does A/AAAA record queries for the specified hostname.
/// @returns    The TTL of the returned records.  If no records were found
///             this will be the negative cache TTL.
int BaseResolver::a_query(const std::string& hostname,
                           int af,
                           std::list<IP46Address>& addrs)
{
  int ttl = 0;

  if (af == AF_INET)
  {
    // Do A record query only.
    DnsResult result = _dns_client->dns_query(hostname, ns_t_a);
    for (std::list<DnsRRecord*>::const_iterator i = result.records().begin();
         i != result.records().end();
         ++i)
    {
      DnsARecord* ar = (DnsARecord*)*i;
      IP46Address addr;
      addr.af = AF_INET;
      addr.addr.ipv4 = ar->address();
      addrs.push_back(addr);
    }
    ttl = result.ttl();
  }
  else if (af == AF_INET6)
  {
    // Do AAAA record query only.
    DnsResult result = _dns_client->dns_query(hostname, ns_t_aaaa);
    for (std::list<DnsRRecord*>::const_iterator i = result.records().begin();
         i != result.records().end();
         ++i)
    {
      DnsAAAARecord* ar = (DnsAAAARecord*)*i;
      IP46Address addr;
      addr.af = AF_INET6;
      addr.addr.ipv6 = ar->address();
      addrs.push_back(addr);
    }
    ttl = result.ttl();
  }
  else
  {
    // Do both A and AAAA record queries in parallel.
    // @TODO
  }

  return ttl;
}

/// Filters the list of addresses combined with the specified port and
/// transport against the blacklist.
/// @returns    The minimum TTL of the relevant blacklist entries.
int BaseResolver::blacklist_filter(std::list<IP46Address>& addrs,
                                   int port,
                                   int transport)
{
  LOG_DEBUG("Filter %d addresses in blacklist, port %d, transport %d",
            addrs.size(), port, transport);
  AddrInfo ai;
  int min_ttl = 0;
  for (std::list<IP46Address>::iterator i = addrs.begin();
       i != addrs.end();
       )
  {
    ai.address = *i;
    ai.port = port;
    ai.transport = transport;

    int ttl = _blacklist->ttl(ai);

    if ((min_ttl == 0) && (min_ttl > ttl))
    {
      min_ttl = ttl;
    }

    if (ttl > 0)
    {
      // This address/port/transport is blacklisted, so remove from the list.
      LOG_DEBUG("Remove blacklisted address/port/transport from list");
      addrs.erase(i++);
    }
    else
    {
      // The address/port/transport is not in the blacklist, so leave it be.
      ++i;
    }
  }
  return min_ttl;
}

void BaseResolver::blacklist(const AddrInfo& ai, int ttl)
{
  char buf[100];
  LOG_DEBUG("Add %s:%d transport %d to blacklist for %d seconds",
            inet_ntop(ai.address.af, &ai.address.addr, buf, sizeof(buf)),
            ai.port, ai.transport, ttl);
  _blacklist->add(ai, true, ttl);
}

/// Selects an address at random from a list.
const IP46Address& BaseResolver::select_address(const std::list<IP46Address>& addrs)
{
  int r = rand() % addrs.size();
  std::list<IP46Address>::const_iterator i = addrs.begin();
  for (int ii = 0; ii < r; ++ii, ++i);

  return *i;
}

/// Parses a target as if it was an IPv4 or IPv6 address and returns the
/// status of the parse.
bool BaseResolver::parse_ip_target(const std::string& target, IP46Address& address)
{
  // Assume the parse fails.
  LOG_DEBUG("Attempt to parse %s as IP address", target.c_str());
  bool rc = false;

  // Strip start and end white-space.
  std::string ip_target = target;
  Utils::trim(ip_target);

  // IPv6 target will be delimited by square brackets, so check for them and
  // strip them if present.
  if ((ip_target[0] == '[')  && (ip_target[ip_target.size() - 1] == ']'))
  {
    ip_target = ip_target.substr(1, ip_target.size() - 2);
    if (inet_pton(AF_INET6, ip_target.c_str(), &address.addr.ipv6) == 1)
    {
      // Parsed the address as a valid IPv6 address.
      address.af = AF_INET6;
      rc = true;
    }
  }
  else
  {
    if (inet_pton(AF_INET, ip_target.c_str(), &address.addr.ipv4) == 1)
    {
      // Parsed the address as a valid IPv4 address.
      address.af = AF_INET;
      rc = true;
    }
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

BaseResolver::NAPTRReplacement* BaseResolver::NAPTRCacheFactory::get(std::string key, int& ttl)
{
  // Iterate NAPTR lookups starting with querying the target domain until
  // we get a terminal result.
  LOG_DEBUG("NAPTR cache factory called for %s", key.c_str());
  NAPTRReplacement* repl = NULL;
  std::string query_key = key;
  int expires = 0;
  bool terminated = false;

  while (!terminated)
  {
    // Issue the NAPTR query.
    LOG_DEBUG("Sending DNS NAPTR query for %s", query_key.c_str());
    DnsResult result = _dns_client->dns_query(query_key, ns_t_naptr);

    if (!result.records().empty())
    {
      // Process the NAPTR records as per RFC2915 section 4.  First step is to
      // collect the records that match one of the requested services and have
      // acceptable flags.
      std::vector<DnsNaptrRecord*> filtered;

      for (std::list<DnsRRecord*>::const_iterator i = result.records().begin();
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
          }
          else
          {
            // This is a terminal record, so set up the result.
            repl = new NAPTRReplacement;
            repl->replacement = replacement;
            repl->flags = naptr->flags();
            repl->transport = _services[naptr->service()];
            ttl = ttl - time(NULL);
            terminated = true;
          }
          break;
        }
      }
    }
    else
    {
      // No NAPTR record found, so return no entry with the default TTL.
      ttl = _default_ttl;
      terminated = true;
    }
  }
  return repl;
}

void BaseResolver::NAPTRCacheFactory::evict(std::string key, NAPTRReplacement* value)
{
  LOG_DEBUG("Evict NAPTR cache %s", key.c_str());
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
    LOG_DEBUG("Split regex into match=%s, replace=%s", match_replace[0].c_str(), match_replace[1].c_str());
    try
    {
      regex.assign(match_replace[0]);
      replace = match_replace[1];
      success = true;
    }
    catch (...)
    {
      success = false;
    }
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

BaseResolver::SRVSelector* BaseResolver::SRVCacheFactory::get(std::string key, int&ttl)
{
  LOG_DEBUG("SRV cache factory called for %s", key.c_str());
  SRVSelector* srv = NULL;
  int expires = 0;

  DnsResult result = _dns_client->dns_query(key, ns_t_srv);

  if (!result.records().empty())
  {
    // We have a result, move the records to a vector and sort it on priority.
    LOG_DEBUG("SRV query returned %d records", result.records().size());
    std::vector<DnsSrvRecord*> sorted;
    for (std::list<DnsRRecord*>::const_iterator i = result.records().begin();
         i != result.records().end();
         ++i)
    {
      sorted.push_back((DnsSrvRecord*)*i);
      if ((expires == 0) ||
          (expires < (*i)->expires()))
      {
        expires = (*i)->expires();
      }
    }

    LOG_DEBUG("Sorting SRV records");
    std::sort(sorted.begin(),
              sorted.end(),
              BaseResolver::SRVCacheFactory::compare_srv_priority);
    LOG_DEBUG("Sorted SRV records");

    // Now rearrange the vector into a vector of vectors for each distinct
    // priority.
    std::vector<std::vector<std::pair<int, std::pair<std::string, int> > > > srv_lists;
    int priority = -1;
    int priority_ix = -1;
    for (std::vector<DnsSrvRecord*>::const_iterator i = sorted.begin();
         i != sorted.end();
         ++i)
    {
      DnsSrvRecord* sr = *i;
      LOG_DEBUG("Record %s:%d priority %d", sr->target().c_str(), sr->port(), sr->priority());
      if (sr->priority() != priority)
      {
        // New priority level, so move to the next entry in the vector.
        LOG_DEBUG("New priority level");
        priority = sr->priority();
        ++priority_ix;
        srv_lists.resize(srv_lists.size() + 1);
        LOG_DEBUG("Added new priority level");
      }

      // Adjust weights.  Any items which have weight 0 are increase to weight
      // of one, and non-zero weights are multiplied by 100.  This gives
      // the right behaviour as per RFC2782 - when all weights are zero we
      // round-robin (but still have the ability to blacklist) and when there
      // are non-zero weights the zero weighted items have a small (but not
      // specified in RFC2782) chance of selection.
      int weight = sr->weight();
      weight = (weight == 0) ? 1 : weight * 100;

      srv_lists[priority_ix].push_back(std::make_pair(weight,
                                                      std::make_pair(sr->target(), sr->port())));
      LOG_DEBUG("Added record to priority vector %d (%d)", priority, priority_ix);
    }

    LOG_DEBUG("Building SRVSelector");
    srv = new SRVSelector(srv_lists);
    LOG_DEBUG("Built SRVSelector");

    ttl = expires - time(NULL);
  }
  else
  {
    // No results from SRV query, so return no entry with the default TTL
    ttl = _default_ttl;
  }

  return srv;
}

void BaseResolver::SRVCacheFactory::evict(std::string key, SRVSelector* value)
{
  LOG_DEBUG("Evict SRV cache %s", key.c_str());
  delete value;
}

bool BaseResolver::SRVCacheFactory::compare_srv_priority(DnsSrvRecord* r1,
                                                         DnsSrvRecord* r2)
{
  return (r1->priority() < r2->priority());
}


