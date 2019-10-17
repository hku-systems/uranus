#ifndef PANOPLY_SYSMMAN_H
#define PANOPLY_SYSMMAN_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

void *ocall_mmap (void *__addr, size_t __len, int __prot, int __flags, int __fd, __off_t __offset);

void *ocall_mmap64 (void *__addr, size_t __len, int __prot, int __flags, int __fd, __off64_t __offset);

int ocall_munmap (void *__addr, size_t __len);

int ocall_mprotect (void *__addr, size_t __len, int __prot);

int ocall_msync (void *__addr, size_t __len, int __flags);

int ocall_mlock (const void *__addr, size_t __len);

int ocall_munlock (const void *__addr, size_t __len);

int ocall_mlockall (int __flags);

int ocall_munlockall (void);

int ocall_mincore (void *__start, size_t __len, unsigned char *__vec);

int ocall_shm_open (const char *__name, int __oflag, mode_t __mode);

int ocall_shm_unlink (const char *__name);

#if defined(__cplusplus)
}
#endif
#endif

