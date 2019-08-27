
#include <sgx_unistd_util.h>
#include <proxy/sgx_unistd_t.h>

int access (const char *__name, int __type) {

}

long int syscall (long int __sysno, ...) {
	long int retval;
	ocall_syscall(&retval, __sysno, 0);
	return retval;
}

int gethostname(char *name, size_t len)
{
	int retval;
	ocall_gethostname(&retval, name, len);
	return retval;
}
int sethostname(const char *name, size_t len)
{
	int retval;
	ocall_sethostname(&retval, name, len);
	return retval;
}

int ftruncate64 (int __fd, __off64_t __length) {
	int retval;
	ocall_ftruncate64(&retval, __fd, __length);
	return retval;
}
 
long lseek(int fd, long offset, int whence)
{
	long retval;
	ocall_lseek(&retval, fd, offset, whence);
	return retval;
}

long lseek64(int fd, long offset, int whence) {
	long retval;
	ocall_lseek(&retval, fd, offset, whence);
	return retval;
}

int fsync(int fd)
{
	int retval;
	ocall_fsync(&retval, fd);
	return retval;
}

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

pid_t getppid(void)
{
	pid_t retval;
	ocall_getppid(&retval);
	return retval;
}

ssize_t pread(int fd, void *buf, size_t nbytes, off_t offset)
{
	ssize_t retval;
	ocall_pread(&retval, fd, buf, nbytes, offset);
	return retval;
}

ssize_t pwrite(int fd, const void *buf, size_t n, off_t offset)
{
	ssize_t retval;
	ocall_pwrite(&retval, fd, buf, n, offset);
	return retval;
}

char *get_current_dir_name(void)
{
	char* retval;
	ocall_get_current_dir_name(&retval);
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

int pipe(int pipedes[2])
{
	int retval;
	ocall_pipe(&retval, pipedes);
	return retval;
}

int pipe2(int pipedes[2], int flag)
{
	int retval;
	ocall_pipe2(&retval, pipedes, flag);
	return retval;
}

int chown(const char *file, uid_t owner, gid_t group)
{
	int retval;
	ocall_chown(&retval, file, owner, group);
	return retval;
}

int fchown(int fd, uid_t owner, gid_t group)
{
	int retval;
	ocall_fchown(&retval, fd, owner, group);
	return retval;
}

int lchown(const char *file, uid_t owner, gid_t group)
{
	int retval;
	ocall_lchown(&retval, file, owner, group);
	return retval;
}

int chdir(const char *path)
{
	int retval;
	ocall_chdir(&retval, path);
	return retval;
}

int fchdir(int fd)
{
	int retval;
	ocall_fchdir(&retval, fd);
	return retval;
}

int dup(int fd)
{
	int retval;
	ocall_dup(&retval, fd);
	return retval;
}

int dup2(int fd, int fd2)
{
	int retval;
	ocall_dup2(&retval, fd, fd2);
	return retval;
}

int dup3(int fd, int fd2, int flags)
{
	int retval;
	ocall_dup3(&retval, fd, fd2, flags);
	return retval;
}
/* Get the real user ID of the calling process.  */
uid_t getuid(void)
{
	uid_t retval;
	ocall_getuid(&retval);
	return retval;
}

/* Get the effective user ID of the calling process.  */
uid_t geteuid(void)
{
	uid_t retval;
	ocall_geteuid(&retval);
	return retval;
}

/* Get the real group ID of the calling process.  */
gid_t getgid(void)
{
	gid_t retval;
	ocall_getgid(&retval);
	return retval;
}

/* Get the effective group ID of the calling process.  */
gid_t getegid(void)
{
	gid_t retval;
	ocall_getegid(&retval);
	return retval;
}

int getpagesize(void)
{
	int retval;
	ocall_getpagesize(&retval);
	return retval;
}

char *getcwd(char *buf, size_t size)
{
	char *retval;
	ocall_getcwd(&retval, buf, size);
	return retval;
}

int unlink(const char *name)
{
	int retval;
	ocall_unlink(&retval, name);
	return retval;
}


int rmdir(const char *name)
{
	int retval;
	ocall_rmdir(&retval, name);
	return retval;
}


void _exit(int status)
{

	// int retval;
	// ocall__exit(&retval, status);
	// return retval;
	ocall__exit(status);
}

long sysconf(int name)
{
	long retval;
	ocall_sysconf(&retval, name);
	return retval;
}



int setuid(uid_t uid)
{
	int retval;
	ocall_setuid(&retval, uid);
	return retval;
}


int setgid(gid_t gid)
{
	int retval;
	ocall_setgid(&retval, gid);
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
