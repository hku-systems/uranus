#ifndef SGX_SYSEVENTFD_UTIL_H
#define SGX_SYSEVENTFD_UTIL_H

#include <struct/sgx_syseventfd_struct.h>
#include <proxy/sgx_syseventfd_t.h>

int eventfd(unsigned int initval, int flags);

#endif

