#include <sgx_grp_util.h>

struct group *getgrgid(gid_t gid)
{
	struct group *retval;
	ocall_getgrgid(&retval, gid);
	return retval;
}


int initgroups(const char *user, gid_t group)
{
	int retval;
	ocall_initgroups(&retval, user, group);
	return retval;
}

