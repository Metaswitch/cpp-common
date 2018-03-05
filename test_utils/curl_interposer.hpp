/**
 * @file curl_interposer.hpp
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <curl/curl.h>
#include <cstdarg>
#include <stdexcept>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <dlfcn.h>
#include <pthread.h>

#include <map>
#include <string>
#include <cstdio>
#include <cerrno>
#include <stdexcept>

#ifndef CURL_INTERPOSER_H
#define CURL_INTERPOSER_H

// Curl manipulation - note that curl is controlled by default
void cwtest_control_curl();
void cwtest_release_curl();

template<typename... Args>
CURLcode proxy_curl_easy_getinfo(CURL* handle, CURLINFO info, Args... args);

template<typename... Args>
CURLcode proxy_curl_easy_setopt(CURL* handle, CURLoption option, Args... args);

#endif
