//
// Created by jyjia on 2020/9/29.
//

#include "sgx_fcntl_util.h"
#include "sgx_dlfcn_util.h"
#include "sgx_sysstat_util.h"
#include "proxy/sgx_fcntl_t.h"

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

void *dlsym (void *handle, const char* name) {
  return NULL;
}

int fstat(int fd, struct stat *buf) {

}
