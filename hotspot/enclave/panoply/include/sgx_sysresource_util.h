#ifndef SGX_SYSRESOURCE_UTIL_H
#define SGX_SYSRESOURCE_UTIL_H

#include <proxy/sgx_sysresource_t.h>

int prlimit (__pid_t pid, enum __rlimit_resource resource, const struct rlimit *new_limit, struct rlimit *old_limit);

int getrlimit (int resource, struct rlimit *new_limit);

int setrlimit (int resource, const struct rlimit *new_limit);


#endif