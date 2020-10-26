#ifndef SGX_POLL_UTIL_H
#define SGX_POLL_UTIL_H

#include <struct/sgx_poll_struct.h>

int poll(struct pollfd* fds, unsigned int nfds, int timeout);

#endif
