
#include <sgx_sysselect_util.h>

int select (int __nfds,  fd_set *__readfds,
    fd_set *__writefds,
    fd_set *__exceptfds,
    struct timeval *__timeout)
{
	int retval;
	ocall_select(&retval, __nfds, __readfds, __writefds, __exceptfds, __timeout);
	return retval;
}

//#define select(A, B, C, D, E) select(A, B, C, D, E)

