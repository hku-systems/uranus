
#include <sgx_syssendfile_util.h>

ssize_t sendfile (int out_fd, int in_fd, off_t *offset, size_t count)
{
	ssize_t retval;
	ocall_sendfile(&retval, out_fd, in_fd, offset, count);
	return retval;
}

