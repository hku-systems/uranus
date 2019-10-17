#ifndef PANOPLY_SIGNAL_H
#define PANOPLY_SIGNAL_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

int ocall_sigemptyset(sigset_t *set);

int ocall_sigfillset(sigset_t *set);

int ocall_sigaddset(sigset_t *set, int signo);

int ocall_sigdelset(sigset_t *set, int signo);

int ocall_sigismember(const sigset_t *set, int signo);

int ocall_sigsuspend(const sigset_t *set);

int ocall_sigaction(int sig, const struct sigaction *act, struct sigaction *oact);

int ocall_sigpending(sigset_t *set);

int ocall_sigwait(const sigset_t *set, int *sig);

__sighandler_t ocall_signal(int __sig, __sighandler_t __handler);

int ocall_raise(int sig);

#if defined(__cplusplus)
}
#endif
#endif


