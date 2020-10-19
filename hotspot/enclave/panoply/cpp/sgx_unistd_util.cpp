
#include <sgx_unistd_util.h>
#include <proxy/sgx_unistd_t.h>
//
//int access (const char *__name, int __type) {
//
//}
//
//long int syscall (long int __sysno, ...) {
//	long int retval;
//	ocall_syscall(&retval, __sysno, 0);
//	return retval;
//}
//
//int gethostname(char *name, size_t len)
//{
//	int retval;
//	ocall_gethostname(&retval, name, len);
//	return retval;
//}
//int sethostname(const char *name, size_t len)
//{
//	int retval;
//	ocall_sethostname(&retval, name, len);
//	return retval;
//}
//
//int ftruncate64 (int __fd, __off64_t __length) {
//	int retval;
//	ocall_ftruncate64(&retval, __fd, __length);
//	return retval;
//}
//
//long lseek(int fd, long offset, int whence)
//{
//	long retval;
//	ocall_lseek(&retval, fd, offset, whence);
//	return retval;
//}
//
//long lseek64(int fd, long offset, int whence) {
//	long retval;
//	ocall_lseek(&retval, fd, offset, whence);
//	return retval;
//}
//
//int fsync(int fd)
//{
//	int retval;
//	ocall_fsync(&retval, fd);
//	return retval;
//}
//
long read(int fd, void *buf, size_t count)
{
	long retval;
	ocall_read(&retval, fd, buf, count);
	return retval;
}

long write(int fd, const void *buf, size_t count)
{
	long retval;
	ocall_write(&retval, fd, buf, count);
	return retval;
}
int close(int fd)
{
	int retval;
	ocall_close(&retval, fd);
	return retval;
}

pid_t getpid(void)
{
	pid_t retval;
	ocall_getpid(&retval);
	return retval;
}

unsigned int sleep(unsigned int seconds)
{
	unsigned int retval;
	ocall_sleep(&retval, seconds);
	return retval;
}

unsigned int usleep(unsigned int seconds)
{
	unsigned int retval;
	ocall_usleep(&retval, seconds);
	return retval;
}

char *strcpy (char *__restrict __dest, const char *__restrict __src) {
	memcpy(__dest, __src, strlen(__src) + 1);
}

char *strcat (char *__restrict __dest, const char *__restrict __src) {
	size_t start = strlen(__dest) + 1;
	memcpy(__dest + start, __src, strlen(__src) + 1);
}



/* Standard file descriptors.  */
#define	STDIN_FILENO	0	/* Standard input.  */
#define	STDOUT_FILENO	1	/* Standard output.  */
#define	STDERR_FILENO	2	/* Standard error output.  */
