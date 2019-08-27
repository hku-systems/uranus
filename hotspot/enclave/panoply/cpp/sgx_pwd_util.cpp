
#include <sgx_pwd_util.h>

struct passwd *getpwuid(uid_t uid)
{
	struct passwd *retval;
	ocall_getpwuid(&retval, uid);
	return retval;
}

struct passwd *getpwnam(const char *name)
{
	struct passwd *retval;
	ocall_getpwnam(&retval, name);
	return retval;
}

int getpwnam_r(const char *name, struct passwd *pwd,
                   char *buf, size_t buflen, struct passwd **result)
{
	int retval;
	ocall_getpwnam_r(&retval, name, pwd, buf, buflen, result);
	return retval;
}
