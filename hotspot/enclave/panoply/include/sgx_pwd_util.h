#ifndef SGX_PWD_UTIL_H
#define SGX_PWD_UTIL_H

#include <struct/sgx_pwd_struct.h>
#include <proxy/sgx_pwd_t.h>

struct passwd *getpwuid(uid_t uid);

struct passwd *getpwnam(const char *name);

int getpwnam_r(const char *name, struct passwd *pwd,
                   char *buf, size_t buflen, struct passwd **result);
      

#endif 