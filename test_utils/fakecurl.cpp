/**
 * @file fakecurl.cpp Fake cURL library for testing.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "fakecurl.hpp"

#include <cstdarg>
#include <stdexcept>
#include <cstring>

using namespace std;

/// Responses to give, by URL.
map<string,Response> fakecurl_responses;

// Responses to give, by URL and body text
map<pair<string,string>,Response> fakecurl_responses_with_body;

/// Requests received, by URL.
map<string,Request> fakecurl_requests;

CURLcode FakeCurl::easy_perform(FakeCurl* curl)
{
  // Save off the request.
  Request req;
  req._method = _method;
  req._headers = _headers;
  req._httpauth = _httpauth;
  req._username = _username;
  req._password = _password;
  req._fresh = _fresh;
  req._body = _body;

  fakecurl_requests[_url] = req;

  // If we've been told how to resolve this URL, do so.
  std::string resolved = _url;
  size_t slash_slash_pos = _url.find("//");
  size_t slash_pos = _url.find('/', slash_slash_pos + 2);
  std::string host_and_port = _url.substr(slash_slash_pos + 2, slash_pos - (slash_slash_pos + 2));
  std::map<std::string, std::string>::iterator it = _resolves.find(host_and_port);
  if (it != _resolves.end())
  {
    resolved = _url.replace(slash_slash_pos + 2, host_and_port.length(), it->second);
  }

  // Check if there's a response ready.
  auto iter = fakecurl_responses.find(resolved);
  auto iter2 = fakecurl_responses_with_body.find(pair<string, string>(resolved, _body));

  Response* resp;
  if (iter != fakecurl_responses.end())
  {
    resp = &iter->second;
  } else if (iter2 != fakecurl_responses_with_body.end()) {
    resp = &iter2->second;
  } else {
    string msg("cURL URL ");
    msg.append(_url).append(" unknown to FakeCurl");
    throw runtime_error(msg);
  }

  // Send the response.
  CURLcode rc;
  curl->_http_rc = resp->_http_rc;

  if ((_debug_callback != NULL) && _verbose)
  {
    // Call the debug callback with some dummy HTTP messages (to exercise SAS
    // logging code).
    std::string text;

    text = _method + " / HTTP/1.1\r\n\r\n" + _body;

    _debug_callback((CURL*)this,
                    CURLINFO_HEADER_OUT,
                    (char*)text.c_str(),
                    text.size(),
                    _debug_data);

    text = "Done request, starting response\n";
    _debug_callback((CURL*)this,
                    CURLINFO_TEXT,
                    (char*)text.c_str(),
                    text.size(),
                    _debug_data);

    text = "HTTP/1.1 200 OK\r\n\r\n" + _body;
    _debug_callback((CURL*)this,
                    CURLINFO_HEADER_IN,
                    (char*)text.c_str(),
                    text.size(),
                    _debug_data);
  }

  if (resp->_code_once != CURLE_OK)
  {
    // Return this code just once.
    rc = resp->_code_once;
    resp->_code_once = CURLE_OK;
  }
  else
  {
    rc = resp->_code;

    if (_writefn != NULL)
    {
      int len = resp->_body.length();
      char* ptr = const_cast<char*>(resp->_body.c_str());
      int handled = _writefn(ptr, 1, len, _writedata);

      if (handled != len)
      {
        throw runtime_error("Write function didn't handle everything");
      }
    }

    if (_hdrfn != NULL)
    {
      for (std::list<string>::const_iterator it = resp->_headers.begin();
           it != resp->_headers.end(); ++it)
      {
        int len = it->length();
        char* ptr = const_cast<char*>(it->c_str());
        int handled = _hdrfn(ptr, 1, len, _hdrdata);

        if (handled != len)
        {
          throw runtime_error("Header function didn't handle everything");
        }
      }
    }
  }

  return rc;
}
