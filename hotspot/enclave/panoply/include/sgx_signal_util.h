#ifndef SGX_SIGNAL_UTIL_H
#define SGX_SIGNAL_UTIL_H

#include <struct/sgx_signal_struct.h>
#include <proxy/sgx_signal_t.h>

int sigemptyset(sigset_t *set);

int sigfillset(sigset_t *set);

int sigaddset(sigset_t *set, int signo);

int sigdelset(sigset_t *set, int signo);

int sigismember(const sigset_t *set, int signo);

int sigsuspend(const sigset_t *set);

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact);

int sigpending(sigset_t *set);

int sigwait(const sigset_t *set, int *sig);

__sighandler_t signal(int __sig, __sighandler_t __handler);

int raise(int signal);

int sigprocmask (int __how, const sigset_t *__restrict __set,
						sigset_t *__restrict __oset);


#endif