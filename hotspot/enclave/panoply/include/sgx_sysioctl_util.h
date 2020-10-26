#ifndef SGX_SYSIOCTL_UTIL_H
#define SGX_SYSIOCTL_UTIL_H

#include "sgx/sys/types.h"

int ioctl(int fd, unsigned long int request, void* arguments);

#endif
