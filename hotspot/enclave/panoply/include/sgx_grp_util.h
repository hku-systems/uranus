#ifndef SGX_GRP_UTIL_H
#define SGX_GRP_UTIL_H

#include "struct/sgx_grp_struct.h"

struct group *getgrgid(gid_t gid);

int initgroups(const char *user, gid_t group);


#endif