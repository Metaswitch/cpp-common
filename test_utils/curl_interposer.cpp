/**
 * @file curl_interposer.hpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2016 Metaswitch Networks Ltd
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

#include "curl_interposer.hpp"
#include "fakecurl.hpp"

static bool control_curl = true;

void cwtest_release_curl()
{
  control_curl = false;
}

void cwtest_control_curl()
{
  control_curl = true;
}

/// Interposed CURL functions
static CURLcode (*real_curl_easy_perform)(CURL* handle);
static const char* (*real_curl_easy_strerror)(CURLcode errnum);
static void (*real_curl_slist_free_all)(struct curl_slist* lst);
static struct curl_slist* (*real_curl_slist_append)(struct curl_slist* lst, const char* str);
static void (*real_curl_easy_cleanup)(CURL* handle);
static CURL* (*real_curl_easy_init)();
static CURLcode (*real_curl_global_init)(long flags);
static CURLcode (*real_curl_easy_getinfo)(CURL* handle, CURLINFO info, ...);
static CURLcode (*real_curl_easy_setopt)(CURL* handle, CURLoption option, ...);

CURLcode curl_easy_perform(CURL* handle)
{
  if (!real_curl_easy_perform)
  {
    real_curl_easy_perform = (CURLcode (*)(CURL*))dlsym(RTLD_NEXT, "curl_easy_perform");
  }

  CURLcode rc;

  if (control_curl)
  {
    FakeCurl* curl = (FakeCurl*)handle;
    rc = curl->easy_perform(curl);
  }
  else
  {
    rc = real_curl_easy_perform(handle);
  }

  return rc;
}

CURLcode curl_global_init(long flags)
{
  if (!real_curl_global_init)
  {
    real_curl_global_init = (CURLcode (*)(long))dlsym(RTLD_NEXT, "curl_global_init");
  }

  CURLcode rc;

  if (control_curl)
  {
    rc = CURLE_OK;
  }
  else
  {
    rc = real_curl_global_init(flags);
  }

  return rc;
}

CURL* curl_easy_init()
{
  if (!real_curl_easy_init)
  {
    real_curl_easy_init = (CURL* (*)())dlsym(RTLD_NEXT, "curl_easy_init");
  }

  CURL* rc;

  if (control_curl)
  {
    FakeCurl* curl = new FakeCurl();
    rc = (CURL*)curl;
  }
  else
  {
    rc = real_curl_easy_init();
  }

  return rc;
}

void curl_easy_cleanup(CURL* handle)
{
  if (!real_curl_easy_cleanup)
  {
    real_curl_easy_cleanup = (void (*)(CURL*))dlsym(RTLD_NEXT, "curl_easy_cleanup");
  }

  if (control_curl)
  {
    FakeCurl* curl = (FakeCurl*)handle;
    delete curl;
  }
  else
  {
    real_curl_easy_cleanup(handle);
  }
}

struct curl_slist* curl_slist_append(struct curl_slist* lst, const char* str)
{
  if (!real_curl_slist_append)
  {
    real_curl_slist_append = (struct curl_slist* (*)(struct curl_slist* lst, const char* str))dlsym(RTLD_NEXT, "curl_slist_append");
  }

  struct curl_slist* rc;

  if (control_curl)
  {
    std::list<std::string>* truelist;

    if (lst == NULL)
    {
      truelist = new std::list<std::string>();
    }
    else
    {
      truelist = (std::list<std::string>*)lst;
    }

    truelist->push_back(str);

    rc = (struct curl_slist*)truelist;
  }
  else
  {
    rc = real_curl_slist_append(lst, str);
  }

  return rc;
}

void curl_slist_free_all(struct curl_slist* lst)
{
  if (!real_curl_slist_free_all)
  {
    real_curl_slist_free_all = (void (*)(struct curl_slist*))dlsym(RTLD_NEXT, "curl_slist_free_all");
  }

  if (control_curl)
  {
    std::list<std::string>* truelist = (std::list<std::string>*)lst;
    delete truelist;
  }
  else
  {
    real_curl_slist_free_all(lst);
  }
}

const char* curl_easy_strerror(CURLcode errnum)
{
  if (!real_curl_easy_strerror)
  {
    real_curl_easy_strerror = (const char* (*)(CURLcode))dlsym(RTLD_NEXT, "curl_easy_strerror");
  }

  const char* rc;

  if (control_curl)
  {
    rc = "Insert error string here";
  }
  else
  {
    rc = real_curl_easy_strerror(errnum);
  }

  return rc;
}

CURLcode curl_easy_setopt(CURL* handle, CURLoption option, ...)
{
  if (!control_curl)
  {
    printf("ERROR! Entered fake curl function when we're not controlling curl");
    return CURLE_OK;
  }

  va_list args;
  va_start(args, option);
  FakeCurl* curl = (FakeCurl*)handle;

  switch (option)
  {
  case CURLOPT_PRIVATE:
  {
    curl->_private = va_arg(args, void*);
  }
  break;
  case CURLOPT_HTTPHEADER:
  {
    struct curl_slist* headers = va_arg(args, struct curl_slist*);
    std::list<std::string>* truelist = (std::list<std::string>*)headers;
    if (truelist != NULL)
      {
      curl->_headers = *truelist;
    }
    else
    {
      curl->_headers.clear();
    }
  }
  break;
  case CURLOPT_URL:
  {
    curl->_url = va_arg(args, char*);
  }
  break;
  case CURLOPT_WRITEFUNCTION:
  {
    curl->_writefn = va_arg(args, datafn_ty);
  }
  break;
  case CURLOPT_WRITEDATA:
  {
    curl->_writedata = va_arg(args, void*);
  }
  break;
  case CURLOPT_FAILONERROR:
  {
    curl->_failonerror = va_arg(args, long);
  }
  break;
  case CURLOPT_HTTPAUTH:
  {
    curl->_httpauth = va_arg(args, long);
  }
  break;
  case CURLOPT_USERNAME:
  {
    curl->_username = va_arg(args, char*);
  }
  break;
  case CURLOPT_PASSWORD:
  {
    curl->_password = va_arg(args, char*);
  }
  break;
  case CURLOPT_PUT:
  {
    if (va_arg(args, long))
    {
      curl->_method = "PUT";
    }
  }
  break;
  case CURLOPT_HTTPGET:
  {
    if (va_arg(args, long))
      {
      curl->_method = "GET";
    }
  }
  break;
  case CURLOPT_POST:
  {
    if (va_arg(args, long))
    {
      curl->_method = "POST";
    }
  }
  break;
  case CURLOPT_CUSTOMREQUEST:
  {
    char* method = va_arg(args, char*);
    if (method != NULL)
    {
      curl->_method = method;
    }
    else
    {
      curl->_method = "GET";
    }
  }
  break;
  case CURLOPT_FRESH_CONNECT:
  {
    curl->_fresh = !!va_arg(args, long);
  }
  break;
  case CURLOPT_HEADERFUNCTION:
  {
    curl->_hdrfn = va_arg(args, headerfn_ty);
  }
  break;
  case CURLOPT_WRITEHEADER:
  {
    curl->_hdrdata = va_arg(args, headerdata_ty);
  }
  break;
  case CURLOPT_POSTFIELDS:
  {
    char* body =  va_arg(args, char*);
    curl->_body = (body == NULL) ? "" : body;
  }
  break;
  case CURLOPT_DEBUGFUNCTION:
  {
    curl->_debug_callback = va_arg(args, debug_callback_t);
  }
  break;
  case CURLOPT_DEBUGDATA:
  {
    curl->_debug_data = va_arg(args, void*);
  }
  break;
  case CURLOPT_VERBOSE:
  {
    long verbose_flag = va_arg(args, long);
    curl->_verbose = (verbose_flag != 0);
  }
  break;
  case CURLOPT_RESOLVE:
  {
    struct curl_slist* hosts = va_arg(args, struct curl_slist*);
    std::list<std::string>* truelist = (std::list<std::string>*)hosts;
    for (std::list<std::string>::iterator it = truelist->begin(); it != truelist->end(); ++it)
    {
      std::string mapping = *it;
      if (mapping.at(0) == '-')
      {
        mapping.erase(0, 1);
        curl->_resolves.erase(mapping);
      }
      else
      {
        size_t first_colon = mapping.find(':');
        size_t second_colon = mapping.find(':', first_colon + 1);
        std::string host = mapping.substr(0, first_colon);
        std::string colon_and_port = mapping.substr(first_colon, second_colon - first_colon);
        std::string ip = mapping.substr(second_colon + 1);
        curl->_resolves[host + colon_and_port] = ip + colon_and_port;
      }
    }
  }
  break;
  case CURLOPT_MAXCONNECTS:
  case CURLOPT_TIMEOUT_MS:
  case CURLOPT_CONNECTTIMEOUT_MS:
  case CURLOPT_DNS_CACHE_TIMEOUT:
  case CURLOPT_TCP_NODELAY:
  case CURLOPT_NOSIGNAL:
  case CURLOPT_READDATA:
  case CURLOPT_READFUNCTION:
  {
    // ignore
  }
  break;
  default:
  {
    throw std::runtime_error("cURL option unknown to FakeCurl");
  }
  break;
  }

  va_end(args);  // http://www.gnu.org/software/gnu-c-manual/gnu-c-manual.html#Variable-Length-Parameter-Lists clarifies that in GCC this does nothing, so is fine even in the presence of exceptions
  return CURLE_OK;
}

CURLcode curl_easy_getinfo(CURL* handle, CURLINFO info, ...)
{
  if (!control_curl)
  {
    printf("ERROR! Entered fake curl function when we're not controlling curl");
    return CURLE_OK;
  }

  va_list args;
  va_start(args, info);
  FakeCurl* curl = (FakeCurl*)handle;

  switch (info)
  {
    case CURLINFO_PRIVATE:
    {
      char** dataptr = va_arg(args, char**);
      *(void**)dataptr = curl->_private;
    }
    break;

    case CURLINFO_PRIMARY_IP:
    {
      static char ip[] = "10.42.42.42";
      char** dataptr = va_arg(args, char**);
      *dataptr = ip;
    }
    break;

    case CURLINFO_PRIMARY_PORT:
    {
      long* dataptr = va_arg(args, long*);
      *dataptr = 80;
    }
    break;

    case CURLINFO_LOCAL_IP:
    {
      static char ip[] = "10.24.24.24";
      char** dataptr = va_arg(args, char**);
      *dataptr = ip;
    }
    break;

    case CURLINFO_LOCAL_PORT:
    {
      long* dataptr = va_arg(args, long*);
      *dataptr = 12345;
    }
    break;

    case CURLINFO_RESPONSE_CODE:
    {
      long* dataptr = va_arg(args, long*);
      *dataptr = curl->_http_rc;
    }
    break;

    default:
    {
      throw std::runtime_error("cURL info unknown to FakeCurl");
    }
  }

  va_end(args);  // http://www.gnu.org/software/gnu-c-manual/gnu-c-manual.html#Variable-Length-Parameter-Lists clarifies that in GCC this does nothing, so is fine even in the presence of exceptions
  return CURLE_OK;
}

template<typename... Args>
CURLcode proxy_curl_easy_getinfo(CURL* handle, CURLINFO info, Args... args)
{
  if (!real_curl_easy_getinfo)
  {
    real_curl_easy_getinfo = (CURLcode (*)(CURL* handle, CURLINFO info, ...))dlsym(RTLD_NEXT, "curl_easy_getinfo");
  }

  if (control_curl)
  {
    printf("ERROR! Entered proxy curl function when we're controlling curl");
    return CURLE_OK;
  }
  else
  {
    return real_curl_easy_getinfo(handle, info, args...);
  }
}

template CURLcode proxy_curl_easy_getinfo<int*>(CURL*, CURLINFO, int*);

template<typename... Args>
CURLcode proxy_curl_easy_setopt(CURL* handle, CURLoption option, Args... args)
{
  if (!real_curl_easy_setopt)
  {
    real_curl_easy_setopt = (CURLcode (*)(CURL* handle, CURLoption option, ...))dlsym(RTLD_NEXT, "curl_easy_setopt");
  }

  if (control_curl)
  {
    printf("ERROR! Entered proxy curl function when we're controlling curl");
    return CURLE_OK;
  }
  else
  {
    return real_curl_easy_setopt(handle, option, args...);
  }
}
template CURLcode proxy_curl_easy_setopt<curl_slist*>(CURL*, CURLoption, curl_slist*);
template CURLcode proxy_curl_easy_setopt<char*>(CURL*, CURLoption, char*);
template CURLcode proxy_curl_easy_setopt<char const*>(CURL*, CURLoption, char const*);
template CURLcode proxy_curl_easy_setopt<std::string*>(CURL*, CURLoption, std::string*);
template CURLcode proxy_curl_easy_setopt<unsigned long (*)(void*, unsigned long, unsigned long, void*)>(CURL*, CURLoption, unsigned long (*)(void*, unsigned long, unsigned long, void*));
