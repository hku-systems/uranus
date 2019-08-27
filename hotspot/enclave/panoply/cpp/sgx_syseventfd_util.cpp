
#include <sgx_syseventfd_util.h>

int eventfd(unsigned int initval, int flags)
{
	int retval;
	ocall_eventfd(&retval, initval, flags);
	return retval;
}
