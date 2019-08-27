#ifndef SGX_SELECT_UTIL_H
#define SGX_SELECT_UTIL_H

#include "proxy/sgx_sysselect_t.h"

int select (int __nfds,  fd_set *__readfds,
    fd_set *__writefds,
    fd_set *__exceptfds,
    struct timeval *__timeout);

#endif
