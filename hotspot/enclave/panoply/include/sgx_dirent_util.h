#ifndef SGX_DIRENT_UTIL_H
#define SGX_DIRENT_UTIL_H

#include <dirent.h>
#include "struct/sgx_dirent_struct.h"
#include "sgx/sys/types.h"
#include "proxy/sgx_dirent_t.h"

typedef struct __dirstream DIR;

DIR *opendir (const char *name);

DIR *fdopendir (int fd);

int closedir (void *dirp_cast);

struct dirent *readdir (void *dirp_cast);

int readdir_r (void *dirp_cast, struct dirent *entry, struct dirent **result);

void rewinddir (void *dirp_cast);

void seekdir (void *dirp_cast, long int pos);

long int telldir ( void *dirp_cast);

int dirfd ( void *dirp_cast);

int alphasort ( const struct dirent **e1,  const struct dirent **e2);

ssize_t getdirentries (int fd, char *buf, size_t nbytes, off_t *basep);

int versionsort (const struct dirent **e1,  const struct dirent **e2);

#endif