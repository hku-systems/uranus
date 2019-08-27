
#include <sgx_sysioctl_util.h>

int ioctl(int fd, unsigned long int request, void* arguments)
{
	int retval;
	ocall_ioctl(&retval, fd, request, arguments);
	return retval;
}

