// #include <replacements/sgx_stdio_util.h>
#ifndef PANOPLY_POOL_H
#define PANOPLY_POOL_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

int ocall_poll(struct pollfd *__fds, nfds_t __nfds, int __timeout);

#if defined(__cplusplus)
}
#endif
#endif