/**
 * @file dnscachedresolver.cpp Implements a DNS caching resolver using C-ARES
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014  Metaswitch Networks Ltd
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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#include <sstream>
#include <iomanip>
#include <fstream>

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "json_parse_utils.h"

#include "log.h"
#include "dnsparser.h"
#include "dnscachedresolver.h"
#include "sas.h"
#include "sasevent.h"
#include "cpp_common_pd_definitions.h"

DnsResult::DnsResult(const std::string& domain,
                     int dnstype,
                     const std::vector<DnsRRecord*>& records,
                     int ttl) :
  _domain(domain),
  _dnstype(dnstype),
  _records(),
  _ttl(ttl)
{
  // Clone the records to the result.
  for (std::vector<DnsRRecord*>::const_iterator i = records.begin();
       i != records.end();
       ++i)
  {
    _records.push_back((*i)->clone());
  }
}

DnsResult::DnsResult(const DnsResult &res) :
  _domain(res._domain),
  _dnstype(res._dnstype),
  _records(),
  _ttl(res._ttl)
{
  // Clone the records to the result.
  for (std::vector<DnsRRecord*>::const_iterator i = res._records.begin();
       i != res._records.end();
       ++i)
  {
    _records.push_back((*i)->clone());
  }
}

DnsResult::DnsResult(DnsResult &&res) :
  _domain(res._domain),
  _dnstype(res._dnstype),
  _records(),
  _ttl(res._ttl)
{
  // Copy the records, then remove them from the source.
  for (std::vector<DnsRRecord*>::const_iterator i = res._records.begin();
       i != res._records.end();
       ++i)
  {
    _records.push_back(*i);
  }

  res._records.clear();
}

DnsResult::DnsResult(const std::string& domain,
                     int dnstype,
                     int ttl) :
  _domain(domain),
  _dnstype(dnstype),
  _records(),
  _ttl(ttl)
{
}

DnsResult::~DnsResult()
{
  while (!_records.empty())
  {
    delete _records.back();
    _records.pop_back();
  }
}

void DnsCachedResolver::init(const std::vector<IP46Address>& dns_servers)
{
  _dns_servers = dns_servers;
  _cache_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

  // Initialize the ares library.  This might have already been done by curl
  // but it's safe to do it twice.
  ares_library_init(ARES_LIB_INIT_ALL);

  // We store a DNSResolver in thread-local data, so create the thread-local
  // store.
  pthread_key_create(&_thread_local, (void(*)(void*))&destroy_dns_channel);

  pthread_condattr_t cond_attr;
  pthread_condattr_init(&cond_attr);
  pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
  pthread_cond_init(&_got_reply_cond, &cond_attr);
  pthread_condattr_destroy(&cond_attr);
}

void DnsCachedResolver::init_from_server_ips(const std::vector<std::string>& dns_servers)
{
  std::vector<IP46Address> dns_server_ips;

  TRC_STATUS("Creating Cached Resolver using servers:");
  for (size_t i = 0; i < dns_servers.size(); i++)
  {
    if (dns_servers[i] == "0.0.0.0")
    {
      // Skip this DNS server
      continue;
    }

    IP46Address addr;
    TRC_STATUS("    %s", dns_servers[i].c_str());
    // Parse the DNS server's IP address.
    if (inet_pton(AF_INET, dns_servers[i].c_str(), &(addr.addr.ipv4)))
    {
      addr.af = AF_INET;
    }
    else if (inet_pton(AF_INET6, dns_servers[i].c_str(), &(addr.addr.ipv6)))
    {
      addr.af = AF_INET6;
    }
    else
    {
      TRC_ERROR("Failed to parse '%s' as IP address - defaulting to 127.0.0.1", dns_servers[i].c_str());
      addr.af = AF_INET;
      (void)inet_aton("127.0.0.1", &(addr.addr.ipv4));
    }
    dns_server_ips.push_back(addr);
  }

  init(dns_server_ips);
}


DnsCachedResolver::DnsCachedResolver(const std::vector<IP46Address>& dns_servers,
                                     const std::string& filename) :
  _port(53),
  _cache(),
  _dns_config_file(filename),
  _static_records()
{
  init(dns_servers);
}

DnsCachedResolver::DnsCachedResolver(const std::vector<std::string>& dns_servers,
                                     const std::string& filename) :
  _port(53),
  _cache(),
  _dns_config_file(filename),
  _static_records()
{
  init_from_server_ips(dns_servers);
}

DnsCachedResolver::DnsCachedResolver(const std::string& dns_server,
                                     int port,
                                     const std::string& filename) :
  _port(port),
  _cache(),
  _dns_config_file(filename),
  _static_records()
{
  init_from_server_ips({dns_server});
}

DnsCachedResolver::~DnsCachedResolver()
{
  DnsChannel* channel = (DnsChannel*)pthread_getspecific(_thread_local);
  if (channel != NULL)
  {
    pthread_setspecific(_thread_local, NULL);
    destroy_dns_channel(channel);
  }
  pthread_key_delete(_thread_local);

  // Clear the cache.
  clear();

  // Delete entries in the _static_records map
  for (const std::pair<std::string, std::vector<DnsRRecord*>>& entry : _static_records)
  {
    for (DnsRRecord* record : entry.second)
    {
      delete record;
    }
  }
}

// Reads static dns records from the specified _dns_config_file.
// The file has the format:
//
// {
//   "hostnames": [
//     {
//       "name": <hostname>,
//       "records": [
//         <record objects>
//       ]
//     }
//   ]
// }
//
// Currently, the only supported record object is CNAME:
//
// {
//   "rrtype": "CNAME",
//   "target": <target>
// }
void DnsCachedResolver::reload_static_records()
{
  if (_dns_config_file == "")
  {
    // No config file specified, just return
    return;
  }

  std::ifstream fs(_dns_config_file.c_str());

  if (!fs)
  {
    TRC_ERROR("DNS config file %s missing", _dns_config_file.c_str());
    CL_DNS_FILE_MISSING.log();
    return;
  }

  TRC_DEBUG("Loading static DNS records from %s",
            _dns_config_file.c_str());

  // File exists and is ready
  std::string dns_config((std::istreambuf_iterator<char>(fs)),
                         std::istreambuf_iterator<char>());

  rapidjson::Document doc;
  doc.Parse<0>(dns_config.c_str());

  if (doc.HasParseError())
  {
    TRC_ERROR("Unable to parse dns config file: %s\nError: %s",
              dns_config.c_str(),
              rapidjson::GetParseError_En(doc.GetParseError()));
    CL_DNS_FILE_MALFORMED.log();
    return;
  }

  try
  {
    std::map<std::string, std::vector<DnsRRecord*>> static_records;

    // We must have a "hostnames" array
    JSON_ASSERT_CONTAINS(doc, "hostnames");
    JSON_ASSERT_ARRAY(doc["hostnames"]);
    rapidjson::Value& hosts_arr = doc["hostnames"];

    for (rapidjson::Value::ConstValueIterator hosts_it = hosts_arr.Begin();
         hosts_it != hosts_arr.End();
         ++hosts_it)
    {
      try
      {
        // We must have a "name" member, which is the hostname
        std::string hostname;
        JSON_GET_STRING_MEMBER(*hosts_it, "name", hostname);

        if (static_records.find(hostname) != static_records.end())
        {
          // Ignore duplicate hostname JSON objects
          TRC_ERROR("Duplicate entry found for hostname %s", hostname.c_str());
          CL_DNS_FILE_DUPLICATES.log();
          continue;
        }

        // We must have a "records" member, which is an array
        JSON_ASSERT_CONTAINS(*hosts_it, "records");
        JSON_ASSERT_ARRAY((*hosts_it)["records"]);
        const rapidjson::Value& records_arr = (*hosts_it)["records"];

        std::vector<DnsRRecord*> records = std::vector<DnsRRecord*>();

        // We only allow one CNAME record per hostname
        bool have_cname = false;

        for (rapidjson::Value::ConstValueIterator records_it = records_arr.Begin();
             records_it != records_arr.End();
             ++records_it)
        {
          try
          {
            // Each record object must have an "rrtype" member
            std::string type;
            JSON_GET_STRING_MEMBER(*records_it, "rrtype", type);

            // Currently, we only support CNAME records
            if (type == "CNAME")
            {
              // CNAME records should have a target, but only one per hostname is allowed
              if (!have_cname)
              {
                std::string target;
                JSON_GET_STRING_MEMBER(*records_it, "target", target);
                DnsCNAMERecord* record = new DnsCNAMERecord(hostname, 0, target);
                records.push_back(record);
                have_cname = true;
              }
              else
              {
                TRC_ERROR("Multiple CNAME entries found for hostname %s", hostname.c_str());
                CL_DNS_FILE_DUPLICATES.log();
              }
            }
            else
            {
              TRC_ERROR("Found unsupported record type: %s", type.c_str());
              CL_DNS_FILE_BAD_ENTRY.log();
            }
          }
          catch (JsonFormatError err)
          {
            TRC_ERROR("Bad DNS record specified for hostname %s in DNS config file %s",
                      hostname.c_str(),
                      _dns_config_file.c_str());
            CL_DNS_FILE_BAD_ENTRY.log();
          }
        }

        static_records.insert(std::make_pair(hostname, records));
      }
      catch (JsonFormatError err)
      {
        TRC_ERROR("Malformed entry in DNS config file %s. Each entry must have "
                  "a \"name\" and \"records\" member",
                  _dns_config_file.c_str());
        CL_DNS_FILE_BAD_ENTRY.log();
      }
    }

    // Now swap out the old _static_records for the new one. This needs to be
    // done under the cache lock
    pthread_mutex_lock(&_cache_lock);
    std::swap(_static_records, static_records);
    pthread_mutex_unlock(&_cache_lock);

    // Finally, clean up the now unused records
    for (const std::pair<std::string, std::vector<DnsRRecord*>>& entry : static_records)
    {
      for (DnsRRecord* record : entry.second)
      {
        delete record;
      }
    }
  }
  catch (JsonFormatError err)
  {
    TRC_ERROR("Error parsing dns config file %s.", _dns_config_file.c_str());
    CL_DNS_FILE_MALFORMED.log();
  }
}

DnsResult DnsCachedResolver::dns_query(const std::string& domain,
                                       int dnstype,
                                       SAS::TrailId trail)
{
  std::vector<DnsResult> res;
  std::vector<std::string> domains;
  domains.push_back(domain);

  dns_query(domains, dnstype, res, trail);

  // The parallel version of dns_query always returns at least one
  // result, so res.front() is safe.
  return res.front();
}

void DnsCachedResolver::dns_query(const std::vector<std::string>& domains,
                                  int dnstype,
                                  std::vector<DnsResult>& results,
                                  SAS::TrailId trail)
{
  std::vector<std::string> new_domains;

  pthread_mutex_lock(&_cache_lock);

  // First, check the _static_records map to see if there are any static records
  // to use in preference to an actual DNS lookup (these are specified in the
  // _dns_config_file)
  for (const std::string& domain : domains)
  {
    std::string new_domain = domain;

    std::map<std::string, std::vector<DnsRRecord*>>::const_iterator map_iter =
      _static_records.find(domain);

    if (map_iter != _static_records.end())
    {
      for (DnsRRecord* record : map_iter->second)
      {
        // Only CNAME records are currently supported
        if (record->rrtype() == ns_t_cname)
        {
          // For static CNAME records, just perform the DNS lookup on the target
          // hostname instead.
          DnsCNAMERecord* cname_record = (DnsCNAMERecord*)record;
          TRC_VERBOSE("Static CNAME record found: %s -> %s",
                      domain.c_str(),
                      cname_record->target().c_str());
          new_domain = cname_record->target();
          break;
        }
      }
    }

    new_domains.push_back(new_domain);
  }
  // Now do the actual lookup
  inner_dns_query(new_domains, dnstype, results, trail);

  pthread_mutex_unlock(&_cache_lock);
}

void DnsCachedResolver::inner_dns_query(const std::vector<std::string>& domains,
                                        int dnstype,
                                        std::vector<DnsResult>& results,
                                        SAS::TrailId trail)
{
  DnsChannel* channel = NULL;

  // We don't lock on cache_lock here because the wrapper function dns_query()
  // already has the lock

  // Expire any cache entries that have passed their TTL.
  expire_cache();

  bool wait_for_query_result = false;
  // First see if any of the domains need to be queried.
  for (std::vector<std::string>::const_iterator domain = domains.begin();
       domain != domains.end();
       ++domain)
  {
    TRC_VERBOSE("Check cache for %s type %d", domain->c_str(), dnstype);
    DnsCacheEntryPtr ce = get_cache_entry(*domain, dnstype);
    time_t now = time(NULL);
    bool do_query = false;
    if (ce == NULL)
    {
      TRC_DEBUG("No entry found in cache");

      // Create an empty record for this cache entry.
      TRC_DEBUG("Create cache entry pending query");
      ce = create_cache_entry(*domain, dnstype);
      do_query = true;
      wait_for_query_result = true;
    }
    else if (ce->expires < now)
    {
      // We have a result, but it has expired.  We should kick off a new
      // asynchronous query to update our DNS cache, unless we have another
      // thread already doing this query for us.

      if (ce->pending_query)
      {
        TRC_DEBUG("Expired entry found in cache - asynchronous query to update it already in progress on another thread");
        // To minimise latency, we should only block until that query returns if
        // we don't have any results - if we have an old result, it's probably
        // still good, so use it.
        if (ce->records.empty())
        {
          wait_for_query_result = true;
        }
      }
      else
      {
        TRC_DEBUG("Expired entry found in cache - starting asynchronous query to update it");
        do_query = true;

        // If we do the query, we should wait for the query result - one thread
        // needs to do so so that the response gets processed.
        wait_for_query_result = true;
      }

    }

    if (do_query)
    {
      if (channel == NULL)
      {
        // Get a DNS channel to issue any queries.
        channel = get_dns_channel();
      }

      if (channel != NULL)
      {
        // DNS server is configured, so create a Transaction for the query
        // and execute it.  Mark the entry as pending and take the lock on
        // it before doing this to prevent any other threads sending the
        // same query.
        TRC_DEBUG("Create and execute DNS query transaction");
        ce->pending_query = true;
        DnsTsx* tsx = new DnsTsx(channel, *domain, dnstype, trail);
        tsx->execute();
      }
    }
  }

  if (channel != NULL && wait_for_query_result)
  {
    // Issued some queries, and at least one of our domains had no current
    // entry in the cache, so wait for the replies before processing the
    // request further.
    TRC_DEBUG("Wait for query responses");
    pthread_mutex_unlock(&_cache_lock);
    wait_for_replies(channel);
    pthread_mutex_lock(&_cache_lock);
    TRC_DEBUG("Received all query responses");
  }

  // We should now have responses for everything (unless another thread was
  // already doing a query), so loop collecting the responses.
  for (std::vector<std::string>::const_iterator i = domains.begin();
       i != domains.end();
       ++i)
  {
    DnsCacheEntryPtr ce = get_cache_entry(*i, dnstype);

    // If we found the cache entry, check whether it is still pending a query.
    while ((ce != NULL) && (ce->pending_query) && wait_for_query_result)
    {
      // We must release the global lock and let the other thread finish
      // the query.
      TRC_DEBUG("Waiting for (non-cached) DNS query for %s", i->c_str());
      pthread_cond_wait(&_got_reply_cond, &_cache_lock);
      ce = get_cache_entry(*i, dnstype);
      TRC_DEBUG("Reawoken from wait for %s type %d", i->c_str(), dnstype);
    }

    if (ce != NULL)
    {
      // Can now pull the information from the cache entry in to the results.
      TRC_DEBUG("Pulling %d records from cache for %s %s",
                ce->records.size(),
                ce->domain.c_str(),
                DnsRRecord::rrtype_to_string(ce->dnstype).c_str());

      int expiry = ce->expires - time(NULL);
      if (expiry < 0)
      {
        // We might have used an expired DNS record to avoid the latency of
        // waiting for a response - if so, don't report a negative TTL.
        expiry = 0;
      }

      results.push_back(DnsResult(ce->domain,
                                  ce->dnstype,
                                  ce->records,
                                  expiry));
    }
    else
    {
      // This shouldn't happen, but if it does, return an empty result set.
      TRC_DEBUG("Return empty result set");
      results.push_back(DnsResult(*i, dnstype, 0));
    }
  }
}

/// Adds or updates an entry in the cache.
void DnsCachedResolver::add_to_cache(const std::string& domain,
                                     int dnstype,
                                     std::vector<DnsRRecord*>& records)
{
  pthread_mutex_lock(&_cache_lock);

  TRC_DEBUG("Adding cache entry %s %s",
            domain.c_str(), DnsRRecord::rrtype_to_string(dnstype).c_str());

  DnsCacheEntryPtr ce = get_cache_entry(domain, dnstype);

  if (ce == NULL)
  {
    // Create a new cache entry.
    TRC_DEBUG("Create cache entry");
    ce = create_cache_entry(domain, dnstype);
  }
  else
  {
    // Clear the existing entry of records.
    clear_cache_entry(ce);
  }

  // Copy all the records across to the cache entry.
  for (size_t ii = 0; ii < records.size(); ++ii)
  {
    add_record_to_cache(ce, records[ii]);
  }

  records.clear();

  // Finally make sure the record is in the expiry list.
  add_to_expiry_list(ce);

  pthread_mutex_unlock(&_cache_lock);
}

/// Renders the current contents of the cache to a displayable string.
std::string DnsCachedResolver::display_cache()
{
  std::ostringstream oss;
  pthread_mutex_lock(&_cache_lock);
  expire_cache();
  int now = time(NULL);
  for (DnsCache::const_iterator i = _cache.begin();
       i != _cache.end();
       ++i)
  {
    DnsCacheEntryPtr ce = i->second;
    oss << "Cache entry " << ce->domain
        << " type=" << DnsRRecord::rrtype_to_string(ce->dnstype)
        << " expires=" << ce->expires-now << std::endl;

    for (std::vector<DnsRRecord*>::const_iterator j = ce->records.begin();
         j != ce->records.end();
         ++j)
    {
      oss << (*j)->to_string() << std::endl;
    }
  }
  pthread_mutex_unlock(&_cache_lock);
  return oss.str();
}

/// Clears the cache.
void DnsCachedResolver::clear()
{
  TRC_DEBUG("Clearing %d cache entries", _cache.size());
  while (!_cache.empty())
  {
    DnsCache::iterator i = _cache.begin();
    DnsCacheEntryPtr ce = i->second;
    TRC_DEBUG("Deleting cache entry %s %s",
              ce->domain.c_str(),
              DnsRRecord::rrtype_to_string(ce->dnstype).c_str());
    clear_cache_entry(ce);
    _cache.erase(i);
  }
}

/// Handles a DNS response from the server.
void DnsCachedResolver::dns_response(const std::string& domain,
                                     int dnstype,
                                     int status,
                                     unsigned char* abuf,
                                     int alen,
                                     SAS::TrailId trail)
{
  pthread_mutex_lock(&_cache_lock);

  TRC_DEBUG("Received DNS response for %s type %s",
            domain.c_str(), DnsRRecord::rrtype_to_string(dnstype).c_str());

  // Stores the domain pointed to by a CNAME record
  std::string canonical_domain;

  // Find the relevant node in the cache.
  DnsCacheEntryPtr ce = get_cache_entry(domain, dnstype);

  // Note that if the request failed or the response failed to parse the expiry
  // time in the cache record is left unchanged.  If it is an existing record
  // it will expire according to the current expiry value, if it is a new
  // record it will expire after DEFAULT_NEGATIVE_CACHE_TTL time.
  if (status == ARES_SUCCESS)
  {
    if (trail != 0)
    {
      SAS::Event event(trail, SASEvent::DNS_SUCCESS, 0);
      event.add_static_param(dnstype);
      event.add_var_param(domain);
      event.add_var_param(alen, abuf);
      SAS::report_event(event);
    }

    // Create a message parser and parse the message.
    DnsParser parser(abuf, alen);

    if (parser.parse())
    {
      // Parsing was successful, so clear out any old records, then process
      // the answers and additional data.
      clear_cache_entry(ce);

      while (!parser.answers().empty())
      {
        DnsRRecord* rr = parser.answers().front();
        parser.answers().pop_front();
        if ((rr->rrtype() == ns_t_a) ||
            (rr->rrtype() == ns_t_aaaa))
        {
          // A/AAAA record, so check that RRNAME matches the question
          // (or a CNAME).
          if ((strcasecmp(rr->rrname().c_str(), domain.c_str()) == 0) ||
              (strcasecmp(rr->rrname().c_str(), canonical_domain.c_str()) == 0))
          {
            // RRNAME matches, so add this record to the cache entry.
            add_record_to_cache(ce, rr);
          }
          else
          {
            TRC_DEBUG("Ignoring A/AAAA record for %s (expecting domain %s)",
                      rr->rrname().c_str(), domain.c_str());
            delete rr;
          }
        }
        else if ((rr->rrtype() == ns_t_srv) ||
                 (rr->rrtype() == ns_t_naptr))
        {
          // SRV or NAPTR record, so add it to the cache entry.
          add_record_to_cache(ce, rr);
        }
        else if (rr->rrtype() == ns_t_cname)
        {
          // Store off the CNAME value, so that if we see subsequent A
          // records for the pointed-to name, we'll recognise them.
          //
          // This only works for responses that contain a CNAME
          // followed by other valid records, e.g.
          // example.net CNAME example.com
          // example.com A 10.0.0.1
          //
          // RFC 1034 mandates this format, so this should be fine.

          canonical_domain = ((DnsCNAMERecord*)rr)->target();
          TRC_DEBUG("CNAME record pointing at %s - treating this as equivalent to %s",
                    canonical_domain.c_str(),
                    domain.c_str());
        }
        else
        {
          TRC_WARNING("Ignoring %s record in DNS answer - only CNAME, A, AAAA, NAPTR and SRV are supported",
                      DnsRRecord::rrtype_to_string(rr->rrtype()).c_str());
          delete rr;
        }
      }

      // Process any additional records returned in the response, creating
      // or updating cache entries.  First we sort the records by cache key.
      std::map<DnsCacheKey, std::list<DnsRRecord*> > sorted;
      while (!parser.additional().empty())
      {
        DnsRRecord* rr = parser.additional().front();
        parser.additional().pop_front();
        if (caching_enabled(rr->rrtype()))
        {
          // Caching is enabled for this record type, so add it to sorted
          // structure.
          sorted[std::make_pair(rr->rrtype(), rr->rrname())].push_back(rr);
        }
        else
        {
          // Caching not enabled for this record, so delete it.
          delete rr;
        }
      }

      // Now update each cache record in turn.
      for (std::map<DnsCacheKey, std::list<DnsRRecord*> >::const_iterator i = sorted.begin();
           i != sorted.end();
           ++i)
      {
        DnsCacheEntryPtr ace = get_cache_entry(i->first.second, i->first.first);
        if (ace == NULL)
        {
          // No existing cache entry, so create one.
          ace = create_cache_entry(i->first.second, i->first.first);
        }
        else
        {
          // Existing cache entry so clear out any existing records.
          clear_cache_entry(ace);
        }
        for (std::list<DnsRRecord*>::const_iterator j = i->second.begin();
             j != i->second.end();
             ++j)
        {
          add_record_to_cache(ace, *j);
        }

        // Finally make sure the record is in the expiry list.
        add_to_expiry_list(ace);
      }
    }
  }
  else
  {
    // If we can't contact the DNS server, keep using the old record's
    // value for an extra 30 seconds.
    //
    // In the event of an extended outage, this will keep being
    // extended for 30 seconds at a time. Note that this only kicks in
    // on DNS server failure, and if a record is deliberately deleted,
    // that will return NXDOMAIN and not be cached.
    TRC_ERROR("Failed to retrieve record for %s: %s", domain.c_str(), ares_strerror(status));

    if (status == ARES_ENOTFOUND)
    {
      // NXDOMAIN, indicating that the DNS entry has been definitively removed
      // (rather than a DNS server failure). Clear the cache for this
      // entry.
      if (trail != 0)
      {
        SAS::Event event(trail, SASEvent::DNS_NOT_FOUND, 0);
        event.add_static_param(dnstype);
        event.add_var_param(domain);
        SAS::report_event(event);
      }

      clear_cache_entry(ce);
    }
    else
    {
      if (trail != 0)
      {
        SAS::Event event(trail, SASEvent::DNS_FAILED, 0);
        event.add_static_param(dnstype);
        event.add_static_param(status);
        event.add_var_param(domain);
        SAS::report_event(event);
      }

      ce->expires = 30 + time(NULL);
    }
  }

  // If there were no records set cache a negative entry to prevent
  // immediate retries.
  if ((ce->records.empty()) &&
      (ce->expires == 0))
  {
    // We didn't get an SOA record, so use a default negative cache timeout.
    ce->expires = DEFAULT_NEGATIVE_CACHE_TTL + time(NULL);
  }

  // Add the record to the expiry list.
  add_to_expiry_list(ce);

  // Flag that the cache entry is no longer pending a query, and release
  // the lock on the cache entry.
  ce->pending_query = false;

  // Another thread may be waiting for our query to finish, so
  // broadcast a signal to wake it up.
  pthread_cond_broadcast(&_got_reply_cond);

  pthread_mutex_unlock(&_cache_lock);
}

/// Returns true if the specified RR type should be cached.
bool DnsCachedResolver::caching_enabled(int rrtype)
{
  return (rrtype == ns_t_a) || (rrtype == ns_t_aaaa) || (rrtype == ns_t_srv) || (rrtype == ns_t_naptr);
}

/// Finds an existing cache entry for the specified domain name and NS type.
DnsCachedResolver::DnsCacheEntryPtr DnsCachedResolver::get_cache_entry(const std::string& domain, int dnstype)
{
  DnsCache::iterator i = _cache.find(std::make_pair(dnstype, domain));

  if (i != _cache.end())
  {
    return i->second;
  }

  return NULL;
}

/// Creates a new empty cache entry for the specified domain name and NS type.
DnsCachedResolver::DnsCacheEntryPtr DnsCachedResolver::create_cache_entry(const std::string& domain, int dnstype)
{
  DnsCacheEntryPtr ce = DnsCacheEntryPtr(new DnsCacheEntry());
  ce->domain = domain;
  ce->dnstype = dnstype;
  ce->expires = 0;
  ce->pending_query = false;
  _cache[std::make_pair(dnstype, domain)] = ce;

  return ce;
}

/// Adds the cache entry to the expiry list.
void DnsCachedResolver::add_to_expiry_list(DnsCacheEntryPtr ce)
{
  int sensible_minimum = 1420070400;  // 1st January 2015
  if ((ce->expires != 0) && (ce->expires < sensible_minimum))
  {
    TRC_WARNING("Cache expiry time is %d - expecting either 0 or an epoch timestamp (> %d)",
                ce->expires,
                sensible_minimum);
  }

  TRC_DEBUG("Adding %s to cache expiry list with deletion time of %d",
            ce->domain.c_str(),
            ce->expires + EXTRA_INVALID_TIME);
  _cache_expiry_list.insert(std::make_pair(ce->expires + EXTRA_INVALID_TIME, std::make_pair(ce->dnstype, ce->domain)));
}

/// Scans for expired cache entries.  In most case records are created then
/// expired, but occasionally a record may be refreshed.  To avoid having
/// to move the record in the expiry list we allow a single record to be
/// reference multiple times in the expiry list, but only expire it when
/// the last reference is reached.
void DnsCachedResolver::expire_cache()
{
  int now = time(NULL);

  while ((!_cache_expiry_list.empty()) &&
         (_cache_expiry_list.begin()->first < now))
  {
    std::multimap<int, DnsCacheKey>::iterator i = _cache_expiry_list.begin();
    TRC_DEBUG("Removing record for %s (type %d, expiry time %d) from the expiry list", i->second.second.c_str(), i->second.first, i->first);

    // Check that the record really is due for expiry and hasn't been
    // refreshed or already deleted.
    DnsCache::iterator j = _cache.find(i->second);
    if (j != _cache.end())
    {
      DnsCacheEntryPtr ce = j->second;

      if (ce->expires + EXTRA_INVALID_TIME == i->first)
      {
        // Record really is ready to expire, so remove it from the main cache
        // map.
        TRC_DEBUG("Expiring record for %s (type %d) from the DNS cache", ce->domain.c_str(), ce->dnstype);
        clear_cache_entry(ce);
        _cache.erase(j);
      }
    }

    _cache_expiry_list.erase(i);
  }
}

/// Clears all the records from a cache entry.
void DnsCachedResolver::clear_cache_entry(DnsCacheEntryPtr ce)
{
  while (!ce->records.empty())
  {
    delete ce->records.back();
    ce->records.pop_back();
  }
  ce->expires = 0;
}

/// Adds a DNS RR to a cache entry.
void DnsCachedResolver::add_record_to_cache(DnsCacheEntryPtr ce, DnsRRecord* rr)
{
  TRC_DEBUG("Adding record to cache entry, TTL=%d, expiry=%ld", rr->ttl(), rr->expires());
  if ((ce->expires == 0) ||
      (ce->expires > rr->expires()))
  {
    TRC_DEBUG("Update cache entry expiry to %ld", rr->expires());
    ce->expires = rr->expires();
  }
  ce->records.push_back(rr);
}

/// Waits for replies to outstanding DNS queries on the specified channel.
void DnsCachedResolver::wait_for_replies(DnsChannel* channel)
{
  // Wait until the expected number of results has been returned.
  while (channel->pending_queries > 0)
  {
    // Call into ares to get details of the sockets it's using.
    ares_socket_t scks[ARES_GETSOCK_MAXNUM];
    int rw_bits = ares_getsock(channel->channel, scks, ARES_GETSOCK_MAXNUM);

    // Translate these sockets into pollfd structures.
    int num_fds = 0;
    struct pollfd fds[ARES_GETSOCK_MAXNUM];
    for (int fd_idx = 0; fd_idx < ARES_GETSOCK_MAXNUM; fd_idx++)
    {
      struct pollfd* fd = &fds[fd_idx];
      fd->fd = scks[fd_idx];
      fd->events = 0;
      fd->revents = 0;
      if (ARES_GETSOCK_READABLE(rw_bits, fd_idx))
      {
        fd->events |= POLLRDNORM | POLLIN;
      }
      if (ARES_GETSOCK_WRITABLE(rw_bits, fd_idx))
      {
        fd->events |= POLLWRNORM | POLLOUT;
      }
      if (fd->events != 0)
      {
        num_fds++;
      }
    }

    // Calculate the timeout.
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    (void)ares_timeout(channel->channel, NULL, &tv);

    // Wait for events on these file descriptors.
    if (poll(fds, num_fds, tv.tv_sec * 1000 + tv.tv_usec / 1000) != 0)
    {
      // We got at least one event, so find which file descriptor(s) this was on.
      for (int fd_idx = 0; fd_idx < num_fds; fd_idx++)
      {
        struct pollfd* fd = &fds[fd_idx];
        if (fd->revents != 0)
        {
          // Call into ares to notify it of the event.  The interface requires
          // that we pass separate file descriptors for read and write events
          // or ARES_SOCKET_BAD if no event has occurred.
          ares_process_fd(channel->channel,
                          fd->revents & (POLLRDNORM | POLLIN) ? fd->fd : ARES_SOCKET_BAD,
                          fd->revents & (POLLWRNORM | POLLOUT) ? fd->fd : ARES_SOCKET_BAD);
        }
      }
    }
    else
    {
      // No events, so just call into ares with no file descriptor to let it handle timeouts.
      ares_process_fd(channel->channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
    }
  }
}

DnsCachedResolver::DnsChannel* DnsCachedResolver::get_dns_channel()
{
  // Get the channel from the thread-local data, or create a new one if none
  // found.
  DnsChannel* channel = (DnsChannel*)pthread_getspecific(_thread_local);
  size_t server_count = _dns_servers.size();
  if (server_count > 3)
  {
    TRC_WARNING("%d DNS servers provided, only using the first 3", _dns_servers.size());
    server_count = 3;
  }

  if ((channel == NULL) &&
      (server_count > 0))
  {
    channel = new DnsChannel;
    channel->pending_queries = 0;
    channel->resolver = this;
    struct ares_options options;

    // ARES_FLAG_STAYOPEN implements TCP keepalive - it doesn't do
    // anything obviously helpful for UDP connections to the DNS server,
    // but it's what we've always tested with so not worth the risk of removing.
    options.flags = ARES_FLAG_STAYOPEN;
    options.timeout = 1000;
    options.tries = server_count;
    options.ndots = 0;
    options.udp_port = _port;
    // We must use ares_set_servers rather than setting it in the options for IPv6 support.
    options.servers = NULL;
    options.nservers = 0;
    ares_init_options(&channel->channel,
                      &options,
                      ARES_OPT_FLAGS |
                      ARES_OPT_TIMEOUTMS |
                      ARES_OPT_TRIES |
                      ARES_OPT_NDOTS |
                      ARES_OPT_UDP_PORT |
                      ARES_OPT_SERVERS);

    // Convert our vector of IP46Addresses into the linked list of
    // ares_addr_nodes which ares_set_servers takes.
    for (size_t ii = 0;
         ii < server_count;
         ii++)
    {
      IP46Address server = _dns_servers[ii];
      struct ares_addr_node* ares_addr = &_ares_addrs[ii];
      memset(ares_addr, 0, sizeof(struct ares_addr_node));
      if (ii > 0)
      {
        int prev_idx = ii - 1;
        _ares_addrs[prev_idx].next = ares_addr;
      }

      ares_addr->family = server.af;
      if (server.af == AF_INET)
      {
        memcpy(&ares_addr->addr.addr4, &server.addr.ipv4, sizeof(ares_addr->addr.addr4));
      }
      else
      {
        memcpy(&ares_addr->addr.addr6, &server.addr.ipv6, sizeof(ares_addr->addr.addr6));
      }
    }

    ares_set_servers(channel->channel, _ares_addrs);

    pthread_setspecific(_thread_local, channel);
  }

  return channel;
}

void DnsCachedResolver::destroy_dns_channel(DnsChannel* channel)
{
  ares_destroy(channel->channel);
  delete channel;
}

DnsCachedResolver::DnsTsx::DnsTsx(DnsChannel* channel, const std::string& domain, int dnstype, SAS::TrailId trail) :
  _channel(channel),
  _domain(domain),
  _dnstype(dnstype),
  _trail(trail)
{
}

DnsCachedResolver::DnsTsx::~DnsTsx()
{
}

void DnsCachedResolver::DnsTsx::execute()
{
  // Note that in error cases, ares_query can call the callback
  // synchronously on the same thread. _cache_lock has to be recursive
  // to account for this (and it's slightly cleaner to increment
  // pending_queries first, to stop it going negative).
  ++_channel->pending_queries;

  if (_trail != 0)
  {
    SAS::Event event(_trail, SASEvent::DNS_LOOKUP, 0);
    event.add_static_param(_dnstype);
    event.add_var_param(_domain);
    SAS::report_event(event);
  }

  ares_query(_channel->channel,
             _domain.c_str(),
             ns_c_in,
             _dnstype,
             DnsTsx::ares_callback,
             this);
}

void DnsCachedResolver::DnsTsx::ares_callback(void* arg,
                                              int status,
                                              int timeouts,
                                              unsigned char* abuf,
                                              int alen)
{
  ((DnsTsx*)arg)->ares_callback(status, timeouts, abuf, alen);
}


void DnsCachedResolver::DnsTsx::ares_callback(int status, int timeouts, unsigned char* abuf, int alen)
{
  _channel->resolver->dns_response(_domain, _dnstype, status, abuf, alen, _trail);
  --_channel->pending_queries;
  delete this;
}

