/**
 * @file dnsparser.h DNS message parser definitions.
 *
 * Copyright (C) Metaswitch Networks 2014
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef DNSPARSER_H__
#define DNSPARSER_H__

#include <string>
#include <list>

#include "dnsrrecords.h"

class DnsParser
{
public:
  DnsParser(unsigned char* buf,
            int length);
  ~DnsParser();

  bool parse();

  std::list<DnsQuestion*>& questions() { return _questions; }
  std::list<DnsRRecord*>& answers() { return _answers; }
  std::list<DnsRRecord*>& authorities() { return _authorities; }
  std::list<DnsRRecord*>& additional() { return _additional; }

  static std::string display_records(const std::list<DnsRRecord*>& records);

private:
  int parse_header(unsigned char* hptr);
  int parse_domain_name(unsigned char* nptr, std::string& name);
  int parse_character_string(unsigned char* sptr, std::string& cstring);
  int parse_question(unsigned char* qptr, DnsQuestion*& question);
  int parse_rr(unsigned char* rptr, DnsRRecord*& record);
  int read_int16(unsigned char* p);
  int read_int32(unsigned char* p);
  int label_length(unsigned char* lptr);
  int label_offset(unsigned char* lptr);
  std::string display_message();

  unsigned char* _data;
  unsigned char* _data_end;
  int _length;

  int _qd_count;
  int _an_count;
  int _ns_count;
  int _ar_count;

  std::list<DnsQuestion*> _questions;
  std::list<DnsRRecord*> _answers;
  std::list<DnsRRecord*> _authorities;
  std::list<DnsRRecord*> _additional;

  // Constants defining sizes and offsets in message header.
  static const int HDR_SIZE                = 12;
  static const int QDCOUNT_OFFSET          = 4;
  static const int ANCOUNT_OFFSET          = 6;
  static const int NSCOUNT_OFFSET          = 8;
  static const int ARCOUNT_OFFSET          = 10;

  // Constants defining sizes and offsets in question.
  static const int Q_FIXED_SIZE            = 4;
  static const int QTYPE_OFFSET            = 0;
  static const int QCLASS_OFFSET           = 2;

  // Constants defining sizes and offsets in common RR header.
  static const int RR_HDR_FIXED_SIZE       = 10;
  static const int RRTYPE_OFFSET           = 0;
  static const int RRCLASS_OFFSET          = 2;
  static const int TTL_OFFSET              = 4;
  static const int RDLENGTH_OFFSET         = 8;

  // Constants defining sizes and offsets in NAPTR record.
  static const int NAPTR_FIXED_SIZE        = 4;
  static const int NAPTR_ORDER_OFFSET      = 0;
  static const int NAPTR_PREFERENCE_OFFSET = 2;
  static const int NAPTR_FLAGS_OFFSET      = 4;

  // Constants defining sizes and offsets in SRV record.
  static const int SRV_FIXED_SIZE          = 6;
  static const int SRV_PRIORITY_OFFSET     = 0;
  static const int SRV_WEIGHT_OFFSET       = 2;
  static const int SRV_PORT_OFFSET         = 4;
  static const int SRV_TARGET_OFFSET       = 6;

};

#endif
