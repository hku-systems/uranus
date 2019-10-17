#include <dirent.h>
#include <stdio.h>

#ifndef PANOPLY_DIRENT_H
#define PANOPLY_DIRENT_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

void *ocall_opendir (const char *name);

void *ocall_fdopendir (int fd);

int ocall_closedir (void *dirp_cast);

struct dirent *ocall_readdir (void *dirp_cast);

int ocall_readdir_r (void *dirp_cast, struct dirent *entry, struct dirent **result);

void ocall_rewinddir (void *dirp_cast);

void ocall_seekdir (void *dirp_cast, long int pos);

long int ocall_telldir ( void *dirp_cast);

int ocall_dirfd ( void *dirp_cast);

int ocall_alphasort ( const struct dirent **e1,  const struct dirent **e2);

ssize_t ocall_getdirentries (int fd, char *buf, size_t nbytes, off_t *basep);

int ocall_versionsort (const struct dirent **e1,  const struct dirent **e2);

#if defined(__cplusplus)
}
#endif
#endif
