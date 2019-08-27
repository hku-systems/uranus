
#include <sgx_utsname_util.h>


int uname(struct utsname *name)
{
	int retval;
	ocall_uname(&retval, name);
	return retval;
}

