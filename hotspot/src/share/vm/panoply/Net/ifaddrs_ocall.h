#ifndef PANOPLY_IFADDRS_H
#define PANOPLY_IFADDRS_H

#include <ifaddrs.h>

#if defined(__cplusplus)
extern "C" {
#endif

int ocall_getifaddrs(struct ifaddrs **ifap);

void ocall_freeifaddrs(struct ifaddrs *ifa);

extern void increase_ocall_count();

#if defined(__cplusplus)
}
#endif
#endif
