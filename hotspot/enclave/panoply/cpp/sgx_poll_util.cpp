#include <sgx_poll_util.h>


int poll(struct pollfd* fds, unsigned int nfds, int timeout)
{
	int ret;
	ocall_poll(&ret, fds, nfds, timeout);
	return ret;
}

