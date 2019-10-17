#ifndef PANOPLY_SYSWAIT_H
#define PANOPLY_SYSWAIT_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

__pid_t ocall_waitpid (__pid_t __pid, int *__stat_loc, int __options);

int ocall_waitid(idtype_t __idtype, __id_t __id, siginfo_t *__infop, int __options);

#if defined(__cplusplus)
}
#endif
#endif
