/**
 * @file dnsrrecords.h  Classes used to represent DNS RRs internally.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef DNSRRECORDS_H__
#define DNSRRECORDS_H__

#include <string>
#include <list>
#include <sstream>
#include <iomanip>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <time.h>

class DnsQuestion
{
public:
  DnsQuestion(const std::string& qname,
              int qtype,
              int qclass) :
    _qname(qname),
    _qtype(qtype),
    _qclass(qclass)
  {
  }

  const std::string& qname() const { return _qname; }
  int qtype() const { return _qtype; }
  int qclass() const { return _qclass; }

private:
  const std::string _qname;
  const int _qtype;
  const int _qclass;
};

class DnsRRecord
{
public:
  DnsRRecord(const std::string& rrname,
             int rrtype,
             int rrclass,
             int ttl) :
    _rrname(rrname),
    _rrtype(rrtype),
    _rrclass(rrclass),
    _ttl(ttl)
  {
    _expires = _ttl + time(NULL);
  }

  virtual ~DnsRRecord()
  {
  }

  const std::string& rrname() const { return _rrname; }
  int rrtype() const { return _rrtype; }
  int rrclass() const { return _rrclass; }
  int ttl() const { return _ttl; }
  int expires() const { return _expires; };
  bool expired() const { return _expires > time(NULL); }

  virtual DnsRRecord* clone()
  {
    return new DnsRRecord(*this);
  }

  virtual std::string to_string() const
  {
    std::ostringstream oss;
    oss << std::setw(23) << std::setfill(' ') << std::left << _rrname << " ";
    oss << std::setw(7) << std::setfill(' ') << std::left << _expires - time(NULL) << " ";
    oss << std::setw(7) << std::setfill(' ') << std::left << rrclass_to_string(_rrclass) << " ";
    oss << std::setw(7) << std::setfill(' ') << std::left << rrtype_to_string(_rrtype);
    return oss.str();
  }

  static std::string rrtype_to_string(int rrtype)
  {
    switch (rrtype)
    {
      case ns_t_a: return "A";
      case ns_t_ns: return "NS";
      case ns_t_cname: return "CNAME";
      case ns_t_soa: return "SOA";
      case ns_t_aaaa: return "AAAA";
      case ns_t_ptr: return "PTR";
      case ns_t_srv: return "SRV";
      case ns_t_naptr: return "NAPTR";
      default:
        break;
    }
    return "Unknown";
  }

  static std::string rrclass_to_string(int rrclass)
  {
    switch (rrclass)
    {
      case ns_c_in: return "IN";
      default:
        break;
    }
    return "Unknown";
  }

private:
  const std::string _rrname;
  int _rrtype;
  int _rrclass;
  int _ttl;
  int _expires;
};

class DnsARecord : public DnsRRecord
{
public:
  DnsARecord(const std::string& rrname, int ttl, const struct in_addr& address) :
    DnsRRecord(rrname, ns_t_a, ns_c_in, ttl),
    _address(address)
  {
  }

  const struct in_addr& address() const { return _address; }

  virtual DnsARecord* clone()
  {
    return new DnsARecord(*this);
  }

  virtual std::string to_string() const
  {
    std::ostringstream oss;
    oss << DnsRRecord::to_string() << " ";
    char buf[100];
    oss << inet_ntop(AF_INET, &_address, buf, sizeof(buf));
    return oss.str();
  }

private:
  const struct in_addr _address;
};

class DnsAAAARecord : public DnsRRecord
{
public:
  DnsAAAARecord(const std::string& rrname, int ttl, const struct in6_addr& address) :
    DnsRRecord(rrname, ns_t_aaaa, ns_c_in, ttl),
    _address(address)
  {
  }

  const struct in6_addr& address() const { return _address; }

  virtual DnsAAAARecord* clone()
  {
    return new DnsAAAARecord(*this);
  }

  virtual std::string to_string() const
  {
    std::ostringstream oss;
    oss << DnsRRecord::to_string() << " ";
    char buf[100];
    oss << inet_ntop(AF_INET6, &_address, buf, sizeof(buf));
    return oss.str();
  }

private:
  const struct in6_addr _address;
};

class DnsSrvRecord : public DnsRRecord
{
public:
  DnsSrvRecord(const std::string& rrname,
               int ttl,
               int priority,
               int weight,
               int port,
               const std::string& target) :
    DnsRRecord(rrname, ns_t_srv, ns_c_in, ttl),
    _priority(priority),
    _weight(weight),
    _port(port),
    _target(target)
  {
  }

  int priority() const { return _priority; }
  int weight() const { return _weight; }
  int port() const { return _port; }
  const std::string& target() const { return _target; }

  virtual DnsSrvRecord* clone()
  {
    return new DnsSrvRecord(*this);
  }

  virtual std::string to_string() const
  {
    std::ostringstream oss;
    oss << DnsRRecord::to_string() << " "
        << _priority << " "
        << _weight << " "
        << _port << " "
        << _target;
    return oss.str();
  }

private:

  const int _priority;
  const int _weight;
  const int _port;
  const std::string _target;
};

class DnsNaptrRecord : public DnsRRecord
{
public:
  DnsNaptrRecord(const std::string& rrname,
                 int ttl,
                 int order,
                 int preference,
                 const std::string& flags,
                 const std::string& service,
                 const std::string& regexp,
                 const std::string& replacement) :
    DnsRRecord(rrname, ns_t_naptr, ns_c_in, ttl),
    _order(order),
    _preference(preference),
    _flags(flags),
    _service(service),
    _regexp(regexp),
    _replacement(replacement)
  {
  }

  int order() const { return _order; }
  int preference() const { return _preference; }
  const std::string& flags() const { return _flags; }
  const std::string& service() const { return _service; }
  const std::string& regexp() const { return _regexp; }
  const std::string& replacement() const { return _replacement; }

  virtual DnsNaptrRecord* clone()
  {
    return new DnsNaptrRecord(*this);
  }

  virtual std::string to_string() const
  {
    std::ostringstream oss;
    oss << DnsRRecord::to_string() << " "
        << _order << " "
        << _preference << " \""
        << _flags << "\" \""
        << _service << "\" \""
        << _regexp << "\" "
        << _replacement;
    return oss.str();
  }

private:
  const int _order;
  const int _preference;
  const std::string _flags;
  const std::string _service;
  const std::string _regexp;
  const std::string _replacement;
};

class DnsCNAMERecord : public DnsRRecord
{
public:
DnsCNAMERecord(const std::string& rrname, int ttl, const std::string& target) :
    DnsRRecord(rrname, ns_t_cname, ns_c_in, ttl),
    _target(target)
  {
  }

  const std::string& target() const { return _target; }

  virtual DnsCNAMERecord* clone()
  {
    return new DnsCNAMERecord(*this);
  }

  virtual std::string to_string() const
  {
    std::ostringstream oss;
    oss << DnsRRecord::to_string() << " " << _target;
    return oss.str();
  }

private:
  const std::string _target;
};

#endif
