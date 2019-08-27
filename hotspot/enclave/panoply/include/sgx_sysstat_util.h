#ifndef SGX_SYSSTAT_UTIL_H
#define SGX_SYSSTAT_UTIL_H

#include <fcntl.h>
#include "proxy/sgx_sysstat_t.h"

int stat(const char *path, struct stat *buf);

int fstat(int fd, struct stat *buf);

int lstat(const char *path, struct stat *buf);

int chmod(const char *file, mode_t mode);

int fchmod(int fd, mode_t mode);

int fchmodat(int fd, const char *file, mode_t mode, int flag);

mode_t umask(mode_t mask);

int mkdir(const char *path, mode_t mode);

int mkdirat(int fd, const char *path, mode_t mode);

int mkfifo(const char *path, mode_t mode);

int mkfifoat(int fd, const char *path, mode_t mode);

int stat64 (const char *__restrict __file,
				   struct stat64 *__restrict __buf);


#endif

