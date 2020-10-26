#ifndef SGX_SYSCTL_UTIL_H
#define SGX_SYSCTL_UTIL_H 1

#include "sgx/sys/types.h"

int sysctl(int *name, int nlen, void *oldval, size_t *oldlenp, void *newval, size_t newlen);

#endif