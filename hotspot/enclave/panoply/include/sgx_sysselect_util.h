#ifndef SGX_SELECT_UTIL_H
#define SGX_SELECT_UTIL_H

#include "struct/sgx_sysselect_struct.h"

int select (int __nfds,  fd_set *__readfds,
    fd_set *__writefds,
    fd_set *__exceptfds,
    struct timeval *__timeout);

#endif
