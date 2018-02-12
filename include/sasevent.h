/**
 * @file sasevent.h
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef SASEVENT_H__
#define SASEVENT_H__

#include <string>
#include "sas.h"

namespace SASEvent {
  // The resource bundle datestamp is updated automatically by Jenkins.  You
  // should not normally edit this value by hand.  If you have to, make sure
  // that you update all of the other code locations that are updated by the
  // Jenkins job "update-sas-resources".
  //
  // !!!DO NOT EDIT THE FOLLOWING LINE MANUALLY!!!
  const std::string CURRENT_RESOURCE_BUNDLE_DATESTAMP = "20180116-applescrumble";
  const std::string RESOURCE_BUNDLE_NAME = "org.projectclearwater";
  const std::string CURRENT_RESOURCE_BUNDLE =
                 RESOURCE_BUNDLE_NAME + "." + CURRENT_RESOURCE_BUNDLE_DATESTAMP;

  // Name of the HTTP header we use to correlate the client and server in SAS.
  const std::string HTTP_BRANCH_HEADER_NAME = "X-SAS-HTTP-Branch-ID";

  // Name of the header used by microservices for trail correlation.
  const std::string HTTP_SPAN_ID = "X-Span-Id";

  // The levels at which clearwater nodes may log HTTP messages.
  enum struct HttpLogLevel
  {
    NONE = 0,
    DETAIL = 40,
    PROTOCOL = 60
  };

  // The type used for the MARKED_ID_SIP_SUBSCRIBE_NOTIFY marker.
  enum SubscribeNotifyType
  {
    SUBSCRIBE = 1,
    NOTIFY = 2
  };

  //----------------------------------------------------------------------------
  // Default compression profiles.
  // PROFILE_* must match compression_profiles.* in
  // https://github.com/Metaswitch/clearwater-sas-resources/blob/master/clearwater_sas_resource_bundle.yaml
  //----------------------------------------------------------------------------
  const SAS::Profile PROFILE_SIP("PCMUACKrportSUBSCRIBEP-Access-Network-Info: BYEusert=0 0 telephone-eventAccept: transportREGISTERSubscription-State: NOTIFYServer: m=audio RTP/AVP c=IN IP4 Expires: 200 OK\r\na=rtpmap:INVITETo: application/sdpVia: Content-Type: From: CSeq: Max-Forwards: Contact: Organization: Content-Length: Call-ID: ;tag=;branch=z9hG4bKSIP/2.0/UDP<sip:", SAS::Profile::Algorithm::LZ4);
  const SAS::Profile PROFILE_HTTP("Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language:\r\nAccept: */*\r\nAuthorization: Digest username=\"\", realm=\"\", nonce=\"\", uri=\"\", response=\"\", opaque=\"\", qop=auth, nc=, cnonce=\"\"\r\nContent-Length: 0\r\nContent-Type: application/vnd.projectclearwater.call-list+xml\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Type: text/html; charset=ISO-8859-1\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Type: text/plain\r\nDELETE /timers/ HTTP/1.1\r\nDate:\r\nEtag: \"\"\r\nGET /impi//registration-status?impu=&visited-network=&auth-type=DEREG HTTP/1.1\r\nGET /impu//location HTTP/1.1\r\nGET /org.etsi.ngn.simservs/users//simservs.xml HTTP/1.1\r\nGET /org.projectclearwater.call-list/users//call-list.xml HTTP/1.1\r\nHTTP/1.1 200 OK\r\nHost: http_backend\r\nLocation: /timers/\r\nPOST /call-id/ HTTP/1.1\r\nPUT /impu//reg-data HTTP/1.1\r\nPUT /impu//reg-data?private_id= HTTP/1.1\r\nServer: cyclone/1.0\r\nUser-Agent:\r\nX-SAS-HTTP-Branch-ID:\r\nX-XCAP-Asserted-Identity:\r\n\"]}}\"}}}}}]}}},\"Acct-Interim-Interval\":{\"aka\":{\"challenge\":\"\",\"Role-Of-Node\":\"}],\"Called-Party-Address\":\"\",\"Calling-Party-Address\":[\"\"}],\"Calling-Party-Address\":[\"\",\"Cause-Code\":\"],\"Cause-Code\":\",\"Content-Length\":,\"Content-Type\":\"\",\"crypt_key\":\"{\"digest\":{\"ha1\":\"{\"event\":{\"Accounting-Record-Type\":,\"Event-Timestamp\":,\"Event-Type\":{\"Expires\":\"],\"Event-Type\":{\"SIP-Method\":\"\"},\"From-Address\":\"{\"impi\":\"\",\"IMS-Charging-Identifier\":\"\",\"IMS-Visited-Network-Identifier\":\"\",\"Instance-Id\":\"\",\"integrity_key\":\"\",\"Inter-Operator-Identifier\":[{\"Originating-IOI\":\"\",\"mandatory-capabilities\":[\"}],\"Message-Body\":[{\"Content-Disposition\":\"\"}],\"Node-Functionality\":\",\"nonce\":\"],\"optional-capabilities\":[\",\"Originator\":\"}}},\"peers\":{\"ccf\":[\"\",\"qop\":\"\",\"realm\":\"{\"reqtype\":\",\"Requested-Party-Address\":\"\",\"response\":\"{\"result-code\":,\"Role-Of-Node\":\",\"Role-Of-Node\":,\"Route-Header-Received\":\",\"Route-Header-Transmitted\":\"\",\"Route-Header-Transmitted\":\",\"scscf\":\",\"Server-Capabilities\":{\"Server-Name\":[\",\"Service-Information\":{\"IMS-Information\":{\"Application-Server-Information\":[{\"Application-Server\":\",\"SIP-Method\":\",\"SIP-Request-Timestamp-Fraction\":,\"SIP-Response-Timestamp\":,\"SIP-Response-Timestamp-Fraction\":\"},\"Subscription-Id\":[{\"Subscription-Id-Data\":\"\",\"Subscription-Id-Type\":\",\"Terminating-IOI\":\"\"]},\"Time-Stamps\":{\"SIP-Request-Timestamp\":\"}},\"User-Name\":\"},\"User-Session-Id\":\"<ClearwaterRegData><RegistrationState><IMSSubscription xsi=\"http://www.w3.org/2001/XMLSchema-instance\" noNamespaceSchemaLocation=\"CxDataType.xsd\"><PrivateID><ServiceProfile><InitialFilterCriteria><TriggerPoint><ConditionTypeCNF><SPT><ConditionNegated><Group><Method><Extension/><ApplicationServer><ServerName><DefaultHandling><PublicIdentity><Identity><ChargingAddresses><CCF priority=\"\"><ECF priority=\"\"><Body present but not logged>", SAS::Profile::Algorithm::LZ4);
  const SAS::Profile PROFILE_SERVICE_PROFILE("<IMSSubscription xsi=\"http://www.w3.org/2001/XMLSchema-instance\" noNamespaceSchemaLocation=\"CxDataType.xsd\"><PrivateID><ServiceProfile><InitialFilterCriteria><TriggerPoint><ConditionTypeCNF><SPT><ConditionNegated><Group><Method><Extension/><ApplicationServer><ServerName><DefaultHandling><PublicIdentity><Identity>", SAS::Profile::Algorithm::LZ4);
  const SAS::Profile PROFILE_LZ4(SAS::Profile::Algorithm::LZ4);

  //----------------------------------------------------------------------------
  // Event spaces.
  //----------------------------------------------------------------------------
  const int COMMON_BASE = 0x000000;
  const int SPROUT_BASE = 0x810000;
  const int HOMESTEAD_BASE = 0x820000;
  const int RALF_BASE = 0x830000;
  const int MEMENTO_BASE = 0x840000;
  const int GEMINI_BASE = 0x850000;
  const int MMTEL_BASE = 0x860000;
  const int MANGELWURZEL_BASE = 0x870000;
  const int CEDAR_BASE = 0x880000;
  const int HOUDINI_BASE = 0x890000;
  const int BIFROST_BASE = 0x8A0000;
  const int WEATHERWAX_BASE = 0x8B0000;
  const int RPE_BASE = 0x8C0000;
  const int CHRONOS_BASE = 0x8D0000;
  const int S4_BASE = 0x8E0000;

  //----------------------------------------------------------------------------
  // Common events and protocol flows.
  //----------------------------------------------------------------------------
  const int RX_SIP_MSG = COMMON_BASE + 0x000001;
  const int TX_SIP_MSG = COMMON_BASE + 0x000002;

  const int TX_HTTP_REQ = COMMON_BASE + 0x000003;
  const int RX_HTTP_REQ = COMMON_BASE + 0x000004;
  const int TX_HTTP_RSP = COMMON_BASE + 0x000005;
  const int RX_HTTP_RSP = COMMON_BASE + 0x000006;
  const int HTTP_REQ_ERROR = COMMON_BASE + 0x000007;
  const int HTTP_REJECTED_OVERLOAD = COMMON_BASE + 0x000008;

  // Duplicates of the above HTTP events, but logged at detail level (40)
  // rather than protocol level (60).
  const int TX_HTTP_REQ_DETAIL = COMMON_BASE + 0x000009;
  const int RX_HTTP_REQ_DETAIL = COMMON_BASE + 0x00000A;
  const int TX_HTTP_RSP_DETAIL = COMMON_BASE + 0x00000B;
  const int RX_HTTP_RSP_DETAIL = COMMON_BASE + 0x00000C;
  const int HTTP_REQ_ERROR_DETAIL = COMMON_BASE + 0x00000D;
  const int HTTP_REJECTED_OVERLOAD_DETAIL = COMMON_BASE + 0x00000E;

  const int DIAMETER_TX = COMMON_BASE + 0x00000F;
  const int DIAMETER_RX = COMMON_BASE + 0x000010;
  const int DIAMETER_TIMEOUT = COMMON_BASE + 0x000011;
  const int DIAMETER_MSG_MISSING_AVP = COMMON_BASE + 0x000012;

  const int HTTP_ABORT = COMMON_BASE + 0x000012;
  const int HTTP_ABORT_DETAIL = COMMON_BASE + 0x000013;

  const int DIAMETER_NO_PEERS = COMMON_BASE + 0x000014;
  const int DIAMETER_NO_CONNECTED_PEERS = COMMON_BASE + 0x000015;

  const int HTTP_BAD_RETRY_AFTER_VALUE = COMMON_BASE + 0x000016;
  const int HTTP_BAD_RETRY_AFTER_VALUE_DETAIL = COMMON_BASE + 0x000017;

  const int HTTP_HOSTNAME_DID_NOT_RESOLVE = COMMON_BASE + 0x000018;
  const int HTTP_HOSTNAME_DID_NOT_RESOLVE_DETAIL = COMMON_BASE + 0x000019;

  const int DIAMETER_MSG_ROUTING_ERROR = COMMON_BASE + 0x00001A;

  const int MEMCACHED_GET_START = COMMON_BASE + 0x000100;
  const int MEMCACHED_GET_SUCCESS = COMMON_BASE + 0x000101;
  const int MEMCACHED_GET_TOMBSTONE = COMMON_BASE + 0x000102;
  const int MEMCACHED_GET_NOT_FOUND = COMMON_BASE + 0x000103;
  const int MEMCACHED_GET_ERROR = COMMON_BASE + 0x000104;
  const int MEMCACHED_SET_START = COMMON_BASE + 0x000105;
  const int MEMCACHED_SET_CONTENTION = COMMON_BASE + 0x000106;
  const int MEMCACHED_SET_FAILED = COMMON_BASE + 0x000107;
  const int MEMCACHED_SET_BLOCKED_BY_TOMBSTONE = COMMON_BASE + 0x000108;
  const int MEMCACHED_SET_BLOCKED_BY_EXPIRED = COMMON_BASE + 0x000109;
  const int MEMCACHED_DELETE = COMMON_BASE + 0x00010A;
  const int MEMCACHED_DELETE_WITH_TOMBSTONE = COMMON_BASE + 0x00010B;
  const int MEMCACHED_DELETE_FAILURE = COMMON_BASE + 0x00010C;
  const int MEMCACHED_NO_HOSTS = COMMON_BASE + 0x00010D;
  const int MEMCACHED_TRY_HOST = COMMON_BASE + 0x00010E;
  const int MEMCACHED_SET_WITHOUT_CAS_START = COMMON_BASE + 0x00010F;
  const int MEMCACHED_GET_WITHOUT_DATA_SUCCESS = COMMON_BASE + 0x000110;
  const int MEMCACHED_SET_WITHOUT_DATA_START = COMMON_BASE + 0x000111;
  const int MEMCACHED_SET_WITHOUT_DATA_OR_CAS_START = COMMON_BASE + 0x000112;
  const int MEMCACHED_REQ_TOO_LARGE = COMMON_BASE + 0x000113;

  const int BASERESOLVE_SRV_RESULT = COMMON_BASE + 0x000200;
  const int BASERESOLVE_A_RESULT_TARGET_SELECT = COMMON_BASE + 0x000201;
  const int DNS_LOOKUP = COMMON_BASE + 0x000202;
  const int DNS_SUCCESS = COMMON_BASE + 0x000203;
  const int DNS_FAILED = COMMON_BASE + 0x000204;
  const int DNS_NOT_FOUND = COMMON_BASE + 0x000205;
  const int DNS_TIMEOUT = COMMON_BASE + 0x000206;
  const int BASERESOLVE_NO_RECORDS = COMMON_BASE + 0x000207;
  const int BASERESOLVE_NO_ALLOWED_RECORDS = COMMON_BASE + 0x000208;
  const int BASERESOLVE_IP_ALLOWED = COMMON_BASE + 0x000209;
  const int BASERESOLVE_IP_NOT_ALLOWED = COMMON_BASE + 0x00020A;
  const int DNS_CACHE_USED = COMMON_BASE + 0x00020B;

  const int CASS_CONNECT_FAIL = COMMON_BASE + 0x0300;
  const int CASS_TIMEOUT = COMMON_BASE + 0x0301;

  const int CASS_REQUEST_TWO_FAIL = COMMON_BASE + 0x0400;

  const int LOAD_MONITOR_ACCEPTED_REQUEST = COMMON_BASE + 0x0500;
  const int LOAD_MONITOR_REJECTED_REQUEST = COMMON_BASE + 0x0501;
  const int LOAD_MONITOR_RECALCULATE_RATE = COMMON_BASE + 0x0502;
  const int LOAD_MONITOR_DECREASE_RATE = COMMON_BASE + 0x0503;
  const int LOAD_MONITOR_DECREASE_PENALTIES = COMMON_BASE + 0x0504;
  const int LOAD_MONITOR_INCREASE_RATE = COMMON_BASE + 0x0505;
  const int LOAD_MONITOR_UNCHANGED_THRESHOLD = COMMON_BASE + 0x0506;
  const int LOAD_MONITOR_UNCHANGED_RATE = COMMON_BASE + 0x0507;
  const int LOAD_MONITOR_UNADJUSTED = COMMON_BASE + 0x0508;

} // namespace SASEvent

#endif
