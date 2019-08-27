//
// Created by max on 12/30/17.
//


#include <sgx_dlfcn_util.h>

void *dlopen (const char *__file, int __mode) {
    void *p;
    ocall_dlopen(&p, __file, __mode);
    return p;
}

int dlclose (void *handle) {
    int ret;
    ocall_dlclose(&ret, handle);
    return ret;
}

void *dlsym (void *handle, const char* name) {
    void *ret;
    ocall_dlsym(&ret, handle, name);
    return ret;
}

int dladdr (const void *__address, Dl_info *__info) {
    int ret;
    ocall_dladdr(&ret, __address,(void*)__info);
    return ret;
}

int dladdr1 (const void *__address, Dl_info *__info,
                                       void **__extra_info, int __flags) {
    int ret;
    ocall_dladdr1(&ret, __address, (void*)__info, __extra_info, __flags);
    return ret;
}

int dlinfo (void * __handle, int __request, void * __arg) {
    int ret;
    ocall_dlinfo(&ret, __handle, __request, __arg);
    return ret;
}
