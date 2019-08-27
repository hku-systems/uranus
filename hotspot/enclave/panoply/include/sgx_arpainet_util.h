#ifndef SGX_ARPAINET_UTIL_H
#define SGX_ARPAINET_UTIL_H

#include <struct/sgx_arpainet_struct.h>
#include <proxy/sgx_arpainet_t.h>

in_addr_t inet_addr (const char *cp);
in_addr_t inet_lnaof (struct in_addr in);

struct in_addr inet_makeaddr (in_addr_t net, in_addr_t host);

in_addr_t inet_netof (struct in_addr in);

in_addr_t inet_network (const char *cp);

char *inet_ntoa (struct in_addr in);

int inet_pton (int af, const char *cp, void *buf);

char *inet_ntop (int af,  const void *cp, char *buf, socklen_t len);

#endif 