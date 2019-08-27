#ifndef SGX_NETIF_UTIL_H
#define SGX_NETIF_UTIL_H

#include <struct/sgx_netif_struct.h>
#include <proxy/sgx_netif_t.h>

unsigned int if_nametoindex(const char *ifname);

char *if_indextoname(unsigned int ifindex, char *ifname);

struct if_nameindex *if_nameindex(void);

void if_freenameindex(struct if_nameindex *ptr);


#endif