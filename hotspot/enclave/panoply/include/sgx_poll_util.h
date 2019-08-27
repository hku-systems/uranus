#ifndef SGX_POLL_UTIL_H
#define SGX_POLL_UTIL_H

#include <struct/sgx_poll_struct.h>
#include <proxy/sgx_poll_t.h>

int poll(struct pollfd* fds, unsigned int nfds, int timeout);

#endif
