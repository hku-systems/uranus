#include <sgx_dirent_util.h>

DIR *opendir (const char *name)
{
	void *retval;
	ocall_opendir(&retval, name);
	return NULL;
}

DIR *fdopendir (int fd)
{
	void *retval;
	ocall_fdopendir(&retval, fd);
	return NULL;
}

int closedir (void *dirp_cast)
{
	int retval;
	ocall_closedir(&retval, dirp_cast);
	return retval;
}

struct dirent *readdir (void *dirp_cast)
{
	struct dirent *retval;
	ocall_readdir(&retval, dirp_cast);
	return retval;
}

int readdir_r (void *dirp_cast, struct dirent *entry, struct dirent **result)
{
	int retval;
	ocall_readdir_r(&retval, dirp_cast, entry, result);
	return retval;
}

void rewinddir (void *dirp_cast)
{
	ocall_rewinddir(dirp_cast);
}

void seekdir (void *dirp_cast, long int pos)
{
	ocall_seekdir(dirp_cast, pos);
}

long int telldir ( void *dirp_cast)
{
	long int retval;
	ocall_telldir(&retval, dirp_cast);
	return retval;
}

int dirfd ( void *dirp_cast)
{
	int retval;
	ocall_dirfd(&retval, dirp_cast);
	return retval;
}

int alphasort ( const struct dirent **e1,  const struct dirent **e2)
{
	int retval;
	ocall_alphasort(&retval, e1, e2);
	return retval;
}

ssize_t getdirentries (int fd, char *buf, size_t nbytes, off_t *basep)
{
	ssize_t retval;
	ocall_getdirentries(&retval,fd, buf, nbytes, basep);
	return retval;
}

int versionsort (const struct dirent **e1,  const struct dirent **e2)
{
	int retval;
	ocall_versionsort(&retval,e1, e2);
	return retval;
}

typedef struct __dirstream DIR;

