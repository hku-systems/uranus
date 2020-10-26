#ifndef SGX_SYSSENDFILE_UTIL_H
#define SGX_SYSSENDFILE_UTIL_H

#include "sgx/sys/types.h"

ssize_t sendfile (int out_fd, int in_fd, off_t *offset, size_t count);

#endif