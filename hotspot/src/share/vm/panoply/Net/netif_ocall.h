

#ifndef PANOPLY_NETIF_H
#define PANOPLY_NETIF_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <net/if.h>

extern void increase_ocall_count();

unsigned int ocall_if_nametoindex(const char *ifname);

char *ocall_if_indextoname(unsigned int ifindex, char *ifname);

struct if_nameindex *ocall_if_nameindex(void);

void ocall_if_freenameindex(struct if_nameindex *ptr);

#if defined(__cplusplus)
}
#endif
#endif

