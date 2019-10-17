#ifndef PANOPLY_SYSUIO_H
#define PANOPLY_SYSUIO_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

void *get_iovec_p(int iovec_id);

int ocall_init_multiple_iovec_outside(const void* __iovec, int iovec_size, int __count);

void ocall_copy_base_to_outside(int iovec_id, int i, const void* base, int len);

ssize_t ocall_readv (int __fd, const void *__iovec, int iovec_size, int __count);

ssize_t ocall_writev (int __fd, int iovec_id, int iovec_size, int __count);

void ocall_free_iovec_outside(int iovec_id, int iovec_size, int __count);

ssize_t ocall_process_vm_readv (pid_t __pid, const struct iovec *__lvec, unsigned long int __liovcnt,
								const struct iovec *__rvec, unsigned long int __riovcnt, unsigned long int __flags);

ssize_t ocall_process_vm_writev (pid_t __pid, const struct iovec *__lvec, unsigned long int __liovcnt,
								 const struct iovec *__rvec, unsigned long int __riovcnt, unsigned long int __flags);

#if defined(__cplusplus)
}
#endif
#endif