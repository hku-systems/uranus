
#include <sgx_sysstat_util.h>

int stat(const char *path, struct stat *buf)
{
	int retval;
	ocall_stat(&retval, path, buf);
	return retval;
}


int fstat(int fd, struct stat *buf)
{
	int retval;
	ocall_fstat(&retval, fd, buf);
	return retval;
}

int lstat(const char *path, struct stat *buf)
{
	int retval;
	ocall_lstat(&retval, path, buf);
	return retval;
}

int chmod(const char *file, mode_t mode)
{
	int retval;
	ocall_chmod(&retval, file, mode);
	return retval;
}

int fchmod(int fd, mode_t mode)
{
	int retval;
	ocall_fchmod(&retval, fd, mode);
	return retval;
}

int fchmodat(int fd, const char *file, mode_t mode, int flag)
{
	int retval;
	ocall_fchmodat(&retval, fd, file, mode, flag);
	return retval;
}

mode_t umask(mode_t mask)
{
	mode_t retval;
	ocall_umask(&retval, mask);
	return retval;
}

int mkdir(const char *path, mode_t mode)
{
	int retval;
	ocall_mkdir(&retval, path, mode);
	return retval;
}

int mkdirat(int fd, const char *path, mode_t mode)
{
	int retval;
	ocall_mkdirat(&retval, fd, path, mode);
	return retval;
}

int mkfifo(const char *path, mode_t mode)
{
	int retval;
	ocall_mkfifo(&retval, path, mode);
	return retval;
}

int mkfifoat(int fd, const char *path, mode_t mode)
{
	int retval;
	ocall_mkfifoat(&retval, fd, path, mode);
	return retval;
}

int stat64 (const char *__restrict __file,
				   struct stat64 *__restrict __buf) {

}

