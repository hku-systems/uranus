#ifndef SGX_SYSMMAN_UTIL_H
#define SGX_SYSMMAN_UTIL_H

#include "proxy/sgx_sysmman_t.h"

void *mmap (void *__addr, size_t __len, int __prot, int __flags, int __fd, __off_t __offset);

void *mmap64 (void *__addr, size_t __len, int __prot, int __flags, int __fd, __off64_t __offset);

int munmap (void *__addr, size_t __len);

int mprotect (void *__addr, size_t __len, int __prot);

int msync (void *__addr, size_t __len, int __flags);

int mlock (const void *__addr, size_t __len);

int munlock (const void *__addr, size_t __len);

int mlockall (int __flags);

int munlockall (void);

int mincore (void *__start, size_t __len, unsigned char *__vec);

int shm_open (const char *__name, int __oflag, mode_t __mode);

int shm_unlink (const char *__name);


#endif
