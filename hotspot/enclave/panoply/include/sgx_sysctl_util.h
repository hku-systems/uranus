#ifndef SGX_SYSCTL_UTIL_H
#define SGX_SYSCTL_UTIL_H 1

#include <proxy/sgx_sysctl_t.h>

int sysctl(int *name, int nlen, void *oldval, size_t *oldlenp, void *newval, size_t newlen);

#endif