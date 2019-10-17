#ifndef PANOPLY_SYSSTAT_H
#define PANOPLY_SYSSTAT_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

int ocall_stat(const char *path, struct stat *buf);

int ocall_fstat(int fd, struct stat *buf);

int ocall_lstat(const char *path, struct stat *buf);

int ocall_chmod (const char *file, mode_t mode);

int ocall_fchmod (int fd, mode_t mode);

int ocall_fchmodat (int fd, const char *file, mode_t mode, int flag);

mode_t ocall_umask (mode_t mask);

int ocall_mkdir (const char *path, mode_t mode);

int ocall_mkdirat (int fd, const char *path, mode_t mode);

int ocall_mkfifo (const char *path, mode_t mode);

int ocall_mkfifoat (int fd, const char *path, mode_t mode);

#if defined(__cplusplus)
}
#endif
#endif