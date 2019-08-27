
#include <sgx_sysresource_util.h>

int prlimit (__pid_t pid, enum __rlimit_resource resource, const struct rlimit *new_limit, struct rlimit *old_limit)
{
	int retval;
	ocall_prlimit(&retval, pid, resource, new_limit, old_limit);
	return retval;
}

int getrlimit (int resource, struct rlimit *new_limit)
{
	int retval;
	ocall_getrlimit(&retval, resource, new_limit);
	return retval;
}

int setrlimit (int resource, const struct rlimit *new_limit)
{
	int retval;
	ocall_setrlimit(&retval, resource, new_limit);
	return retval;
}

