//
// Created by max on 12/30/17.
//

#ifndef HOTSPOT_DLFCN_OCALL_H
#define HOTSPOT_DLFCN_OCALL_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <dlfcn.h>

void *ocall_dlopen (const char *__file, int __mode);
int ocall_dlclose (void *__handle);
void *ocall_dlsym (void * __handle, const char * __name);
int ocall_dladdr (const void *__address, void *__info);
int ocall_dladdr1 (const void *__address, void *__info,
void **__extra_info, int __flags);
int ocall_dlinfo (void * __handle, int __request, void * __arg);
#if defined(__cplusplus)
}
#endif

#endif //HOTSPOT_DLFCN_OCALL_H
