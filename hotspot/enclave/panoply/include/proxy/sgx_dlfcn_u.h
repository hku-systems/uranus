#ifndef SGX_DLFCN_U_H__
#define SGX_DLFCN_U_H__

#include <stdint.h>
#include <wchar.h>
#include <stddef.h>
#include <string.h>
#include "sgx_edger8r.h" /* for sgx_satus_t etc. */


#include <stdlib.h> /* for size_t */

#define SGX_CAST(type, item) ((type)(item))

#ifdef __cplusplus
extern "C" {
#endif

void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dlopen, (const char* __file, int __mode));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dlclose, (void* __handle));
void* SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dlsym, (void* __handle, const char* __name));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dladdr, (const void* __address, void* __info));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dladdr1, (const void* __address, void* __info, void** __extra_info, int __flags));
int SGX_UBRIDGE(SGX_NOCONVENTION, ocall_dlinfo, (void* __handle, int __request, void* __arg));


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
