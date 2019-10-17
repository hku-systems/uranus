
#ifndef PANOPLY_ARPAINET_H
#define PANOPLY_ARPAINET_H
#if defined(__cplusplus)
extern "C" {
#endif
extern void increase_ocall_count();

in_addr_t ocall_inet_addr (const char *cp);

in_addr_t ocall_inet_lnaof (struct in_addr in);

struct in_addr ocall_inet_makeaddr (in_addr_t net, in_addr_t host);

in_addr_t ocall_inet_netof (struct in_addr in);

in_addr_t ocall_inet_network (const char *cp);

char *ocall_inet_ntoa (struct in_addr in);

int ocall_inet_pton (int af, const char *cp, void *buf);

char *ocall_inet_ntop (int af,  const void *cp, char *buf, socklen_t len);

#if defined(__cplusplus)
}
#endif
#endif