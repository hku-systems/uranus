#ifndef SGX_IFADDRS_UTIL_H
#define SGX_IFADDRS_UTIL_H

#include <struct/sgx_ifaddrs_struct.h>
#include <proxy/sgx_ifaddrs_t.h>


int getifaddrs(struct ifaddrs **ifap);

void freeifaddrs(struct ifaddrs *ifa);
#endif