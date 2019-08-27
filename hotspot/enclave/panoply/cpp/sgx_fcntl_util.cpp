#include <sgx_fcntl_util.h>

int open(const char *pathname, int flags, ...)
{
    va_list ap; 
    mode_t mode = 0;
 	va_start(ap, flags); 
    if (flags & O_CREAT)
        mode = va_arg(ap, mode_t); 
    else
        mode = 0777;
    va_end(ap); 	
	int retval;
	ocall_open2(&retval, pathname, flags, mode);
	return retval;
}

int creat(const char *pathname, unsigned int mode)
{
	int retval;
	ocall_creat(&retval, pathname, mode);
	return retval;
}

int openat(int dirfd, const char *pathname, int flags, ...)
{
    va_list ap; 
    mode_t mode = 0;
 	va_start(ap, flags); 
    if (flags & O_CREAT)
        mode = va_arg(ap, mode_t); 
    else
        mode = 0777;
    va_end(ap); 
	int retval;
	ocall_openat2(&retval, dirfd, pathname, flags, mode);
	return retval;	
}

int fcntl(int fd, int cmd, ...)
{
	va_list ap;
	int retval;
	va_start(ap, cmd); 
	long larg = -1;
	struct flock *flarg = NULL;
	// Fix me: Should refer to the linux kernel in order to do it in the right way
	switch(cmd)	{
		case F_GETFD:
		case F_GETFL:
		case F_GETOWN:
			va_end(ap);
			ocall_fcntl1(&retval, fd, cmd);
			return retval;
		case F_DUPFD:
		case F_DUPFD_CLOEXEC:
		case F_SETFD:
		case F_SETFL:
		case F_SETOWN:
			larg = va_arg(ap, long);
			// fprintf(stderr, "fcntl setfd or setfl with flag: %d \n", larg);
			ocall_fcntl2(&retval, fd, cmd, larg);
			return retval;
		case F_SETLK:
		case F_GETLK:
		case F_SETLKW:
			flarg = va_arg(ap, struct flock *);
			// fprintf(stderr, "fcntl setfd or setfl with flock: %p \n", flarg);
			ocall_fcntl3(&retval, fd, cmd, flarg, sizeof(struct flock));
			return retval;
		default:
			// fprintf(stderr, "CMD is not supported by the wrapper in fcntl\n");
			va_end(ap);
			return -1;
	};
	
	return -1;
}