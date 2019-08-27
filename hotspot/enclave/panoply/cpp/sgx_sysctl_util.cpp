
#include <sgx_sysctl_util.h>

int sysctl(int *name, int nlen, void *oldval, size_t *oldlenp, void *newval, size_t newlen)
{
	int retval;
	ocall_sysctl(&retval, name, nlen, oldval, oldlenp, newval, newlen);
	return retval;
}
