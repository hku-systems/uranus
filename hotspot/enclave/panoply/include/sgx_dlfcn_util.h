//
// Created by max on 12/30/17.
//

#ifndef HOTSPOT_SGX_DLFCN_UTIL_H
#define HOTSPOT_SGX_DLFCN_UTIL_H

#include "struct/sgx_dlfcn_struct.h"
#include "proxy/sgx_dlfcn_t.h"

void *dlopen (const char *__file, int __mode);

int dlclose (void *handle);

void *dlsym (void *handle, const char* name);

int dladdr (const void *__address, Dl_info *__info);

int dladdr1 (const void *__address, Dl_info *__info,
                                       void **__extra_info, int __flags);

int dlinfo (void * __handle, int __request, void * __arg);

#endif //HOTSPOT_SGX_DLFCN_UTIL_H
