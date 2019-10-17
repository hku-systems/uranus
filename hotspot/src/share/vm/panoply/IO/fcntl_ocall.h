#ifndef PANOPLY_FCNTL_H
#define PANOPLY_FCNTL_H



#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

int ocall_open1(const char *pathname, int flags);

int ocall_open2(const char *pathname, int flags, unsigned int mode);

int ocall_creat(const char *pathname, unsigned int mode);

int ocall_openat1(int dirfd, const char *pathname, int flags);

int ocall_openat2(int dirfd, const char *pathname, int flags, unsigned int mode);

int ocall_fcntl1(int fd, int cmd);

int ocall_fcntl2(int fd, int cmd, long arg);

int ocall_fcntl3(int fd, int cmd, void* arg_cast, int flocksz);

#if defined(__cplusplus)
}
#endif
#endif


