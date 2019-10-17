#include <sys/utsname.h>

#include "utsname_ocall.h"

int ocall_uname(struct utsname *name)
{
    increase_ocall_count();
	return uname(name);
}