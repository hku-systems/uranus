
#include <proxy/sgx_syswait_t.h>

__pid_t waitpid(__pid_t __pid, int *__stat_loc, int __options)
{
	__pid_t retval;
	ocall_waitpid(&retval, __pid, __stat_loc, __options);
	return retval;
}

int waitid(idtype_t __idtype, __id_t __id, siginfo_t *__infop, int __options)
{
	int retval;
	ocall_waitid(&retval, __idtype, __id, __infop, __options);
	return retval;
}

