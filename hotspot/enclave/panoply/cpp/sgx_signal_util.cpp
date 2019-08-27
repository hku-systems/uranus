
#include <sgx_signal_util.h>

int sigemptyset(sigset_t *set)
{
	int retval;
	ocall_sigemptyset(&retval, set);
	return retval;
}

int sigfillset(sigset_t *set)
{
	int retval;
	ocall_sigfillset(&retval, set);
	return retval;
}

int sigaddset(sigset_t *set, int signo)
{
	int retval;
	ocall_sigaddset(&retval, set, signo);
	return retval;
}

int sigdelset(sigset_t *set, int signo)
{
	int retval;
	ocall_sigdelset(&retval, set, signo);
	return retval;
}

int sigismember(const sigset_t *set, int signo)
{
	int retval;
	ocall_sigismember(&retval, set, signo);
	return retval;
}

int sigsuspend(const sigset_t *set)
{
	int retval;
	ocall_sigsuspend(&retval, set);
	return retval;
}

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
	int retval;
	ocall_sigaction(&retval, sig, act, oact);
	return retval;
}

int sigpending(sigset_t *set)
{
	int retval;
	ocall_sigpending(&retval, set);
	return retval;
}

int sigwait(const sigset_t *set, int *sig)
{
	int retval;
	ocall_sigwait(&retval, set, sig);
	return retval;
}

__sighandler_t signal(int __sig, __sighandler_t __handler)
{
	__sighandler_t retval;
	ocall_signal(&retval, __sig, __handler);
	return retval;
}

int raise(int signal)
{
	int retval;
	ocall_raise(&retval, signal);
	return retval;
}

int sigprocmask (int __how, const sigset_t *__restrict __set,
						sigset_t *__restrict __oset) {

}
