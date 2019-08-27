#ifndef SGX_SYSWAIT_UTIL_H
#define SGX_SYSWAIT_UTIL_H

#include <proxy/sgx_syswait_t.h>

__pid_t waitpid(__pid_t __pid, int *__stat_loc, int __options);

int waitid(idtype_t __idtype, __id_t __id, siginfo_t *__infop, int __options);

//#define waitpid(A, B, C) sgx_wrapper_waitpid(A, B, C)

#endif 