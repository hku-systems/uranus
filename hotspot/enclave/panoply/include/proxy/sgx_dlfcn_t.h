#ifndef SGX_DLFCN_T_H__
#define SGX_DLFCN_T_H__

#include <stdint.h>
#include <wchar.h>
#include <stddef.h>
#include "sgx_edger8r.h" /* for sgx_ocall etc. */


#include <stdlib.h> /* for size_t */

#define SGX_CAST(type, item) ((type)(item))

#ifdef __cplusplus
extern "C" {
#endif



sgx_status_t SGX_CDECL ocall_dlopen(void** retval, const char* __file, int __mode);
sgx_status_t SGX_CDECL ocall_dlclose(int* retval, void* __handle);
sgx_status_t SGX_CDECL ocall_dlsym(void** retval, void* __handle, const char* __name);
sgx_status_t SGX_CDECL ocall_dladdr(int* retval, const void* __address, void* __info);
sgx_status_t SGX_CDECL ocall_dladdr1(int* retval, const void* __address, void* __info, void** __extra_info, int __flags);
sgx_status_t SGX_CDECL ocall_dlinfo(int* retval, void* __handle, int __request, void* __arg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
