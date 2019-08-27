
#include <sgx_sysmman_util.h>

void *mmap (void *__addr, size_t __len, int __prot, int __flags, int __fd, __off_t __offset)
{
	void* retval;
	ocall_mmap(&retval, __addr, __len, __prot, __flags, __fd, __offset);
	return retval;
}

void *mmap64 (void *__addr, size_t __len, int __prot, int __flags, int __fd, __off64_t __offset)
{
	void* retval;
	ocall_mmap64(&retval, __addr, __len, __prot, __flags, __fd, __offset);
	return retval;
}

int munmap (void *__addr, size_t __len)
{
	int retval;
	ocall_munmap(&retval, __addr, __len);
	return retval;
}

int mprotect (void *__addr, size_t __len, int __prot)
{
	int retval;
	ocall_mprotect(&retval, __addr, __len, __prot);
	return retval;
}

int msync (void *__addr, size_t __len, int __flags)
{
	int retval;
	ocall_msync(&retval, __addr, __len, __flags);
	return retval;
}

int mlock (const void *__addr, size_t __len)
{
	int retval;
	ocall_mlock(&retval, __addr, __len);
	return retval;
}

int munlock (const void *__addr, size_t __len)
{
	int retval;
	ocall_munlock(&retval, __addr, __len);
	return retval;
}

int mlockall (int __flags)
{
	int retval;
	ocall_mlockall(&retval, __flags);
	return retval;
}

int munlockall (void)
{
	int retval;
	ocall_munlockall(&retval);
	return retval;
}

int mincore (void *__start, size_t __len, unsigned char *__vec)
{
	int retval;
	ocall_mincore(&retval, __start, __len, __vec);
	return retval;
}

int shm_open (const char *__name, int __oflag, mode_t __mode)
{
	int retval;
	// fprintf(stderr, "Fix me: Dummy shm_open call \n");
	ocall_shm_open(&retval, __name, __oflag, __mode);
	return retval;
}

int shm_unlink (const char *__name)
{
	int retval;
	// fprintf(stderr, "Fix me: Dummy shm_open call \n");
	ocall_shm_unlink(&retval, __name);
	return retval;
}
