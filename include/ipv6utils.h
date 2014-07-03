#ifndef IPV6UTILS_H_
#define IPV6UTILS_H_

#include <arpa/inet.h>

bool is_ipv6(std::string address)
{
// Determine if we're IPv4 or IPv6.
  int http_af = AF_INET;
  struct in6_addr dummy_addr;
  if (inet_pton(AF_INET6, address.c_str(), &dummy_addr) == 1)
  {
    http_af = AF_INET6;
  }
  return (http_af == AF_INET6);
}

#endif
