/**
 * @file fakecurl.hpp Fake cURL library header for testing.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>
#include <list>
#include <map>

#include <curl/curl.h>

typedef std::map<std::string,std::string>* headerdata_ty;
typedef size_t (*datafn_ty)(void* ptr, size_t size, size_t nmemb, void* userdata);
typedef size_t (*headerfn_ty)(void* ptr, size_t size, size_t nmemb, headerdata_ty headers);
typedef int (*debug_callback_t)(CURL *handle,
                                curl_infotype type,
                                char *data,
                                size_t size,
                                void *userptr);
typedef curl_socket_t (socket_callback_t)(void *context,
                                           curlsocktype purpose,
                                           struct curl_sockaddr *address);
typedef int (sockopt_callback_t)(void *context,
                                  curl_socket_t curlfd,
                                  curlsocktype purpose);

/// The content of a request.
class Request
{
public:
  std::string _method;
  std::list<std::string> _headers;
  std::string _body;
  long _httpauth; //^ OR of CURLAUTH_ constants
  long _timeout_ms;
  std::string _username;
  std::string _password;
  bool _fresh;
};

/// The content of a response.
class Response
{
public:
  CURLcode _code_once;  //< If not CURLE_OK, issue this code first then the other.
  CURLcode _code;  //< cURL easy doesn't accept HTTP status codes
  std::string _body;
  std::list<std::string> _headers;
  int _http_rc;

  Response() :
    _code_once(CURLE_OK),
    _code(CURLE_OK),
    _body(""),
    _http_rc(200)
  {
  }

  Response(const std::string& body) :
    _code_once(CURLE_OK),
    _code(CURLE_OK),
    _body(body),
    _http_rc(200)
  {
  }

  Response(CURLcode code_once, const std::string& body) :
    _code_once(code_once),
    _code(CURLE_OK),
    _body(body),
    _http_rc(200)
  {
  }

  Response(std::list<std::string> headers) :
    _code_once(CURLE_OK),
    _code(CURLE_OK),
    _body(""),
    _headers(headers),
    _http_rc(200)
  {
  }

  Response(const char* body) :
    _code_once(CURLE_OK),
    _code(CURLE_OK),
    _body(body),
    _http_rc(200)
  {
  }

  Response(CURLcode code) :
    _code_once(CURLE_OK),
    _code(code),
    _body(""),
    _http_rc(200)
  {
  }

  Response(int http_rc) :
    _code_once(CURLE_OK),
    _code(CURLE_OK),
    _body(""),
    _http_rc(http_rc)
  {
  }

  Response(int http_rc, const std::string& body) :
    _code_once(CURLE_OK),
    _code(CURLE_OK),
    _body(body),
    _http_rc(http_rc)
  {
  }

  Response(int http_rc, std::list<std::string> headers) :
    _code_once(CURLE_OK),
    _code(CURLE_OK),
    _body(""),
    _headers(headers),
    _http_rc(http_rc)
  {
  }
};

/// Object representing a single fake cURL handle.
class FakeCurl
{
public:
  std::string _method;
  std::string _url;

  std::list<std::string> _headers;

  bool _failonerror;
  long _httpauth;  //^ OR of CURLAUTH_* constants
  long _timeout_ms;
  std::string _username;
  std::string _password;
  bool _fresh;

  // Map of hostname + port -> IP address + port, as configured with
  // CURLOPT_RESOLVE.
  std::map<std::string, std::string> _resolves;

  datafn_ty _readfn;
  void* _readdata; //^ user data; not owned by this object

  std::string _body;
  datafn_ty _writefn;
  void* _writedata; //^ user data; not owned by this object

  headerfn_ty _hdrfn;
  headerdata_ty _hdrdata; //^ user data; not owned by this object

  void* _private;

  debug_callback_t _debug_callback;
  void* _debug_data;
  bool _verbose;

  int _http_rc;

  socket_callback_t* _socket_callback;
  sockopt_callback_t* _sockopt_callback;
  void* _socket_data;

  FakeCurl() :
    _method("GET"),
    _failonerror(false),
    _httpauth(0L),
    _fresh(false),
    _readfn(NULL),
    _readdata(NULL),
    _writefn(NULL),
    _writedata(NULL),
    _hdrfn(NULL),
    _hdrdata(NULL),
    _private(NULL),
    _debug_callback(NULL),
    _debug_data(NULL),
    _http_rc(200),
    _socket_callback(NULL),
    _sockopt_callback(NULL),
    _socket_data(NULL)
  {
  }

  virtual ~FakeCurl()
  {
  }

  CURLcode easy_perform(FakeCurl* curl);
};

/// Responses to give, by URL.
extern std::map<std::string,Response> fakecurl_responses;
extern std::map<std::pair<std::string, std::string>, Response> fakecurl_responses_with_body;

/// Requests received, by URL.
extern std::map<std::string,Request> fakecurl_requests;
