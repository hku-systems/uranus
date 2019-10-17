//
// Created by max on 12/30/17.
//

#include "dlfcn_ocall.h"
#include <dlfcn.h>

void *ocall_dlopen (const char *__file, int __mode) {
    return dlopen(__file, __mode);
}
int ocall_dlclose (void *__handle) {
    return dlclose(__handle);
}
void *ocall_dlsym (void * __handle, const char * __name) {
    return dlsym(__handle, __name);
}
int ocall_dladdr (const void *__address, void* __info) {
    return dladdr(__address, (Dl_info*)__info);
}
int ocall_dladdr1 (const void *__address, void *__info,
                   void **__extra_info, int __flags) {
    return dladdr1(__address, (Dl_info*)__info, __extra_info, __flags);
}
int ocall_dlinfo (void * __handle, int __request, void * __arg) {
    return dlinfo(__handle, __request, __arg);
}