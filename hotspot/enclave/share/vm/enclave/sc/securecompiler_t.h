#ifndef LINUX_SGX_SIM_enclave/share/vm/enclave/sc/securecompiler_T_H
#define LINUX_SGX_SIM_enclave/share/vm/enclave/sc/securecompiler_T_H

#ifndef  __cplusplus
typedef int bool;
#endif

#ifdef  __cplusplus
extern "C" {
#endif

#include <sgx_trts.h>
sgx_status_t sgx_c1_initialize(void* pms);

sgx_status_t sgx_gc_scavenge(void* pms);

sgx_status_t sgx_interpreted_entry_zero_locals(void* pms);

sgx_status_t SGX_CDECL ocall_interpreter(void** retval, void* r14, int size, void* method, void* thread, void* sender);

sgx_status_t SGX_CDECL ocall_jvm_resolve_invoke(int byte, void* mh, int bci, void* recv, int idx, void* bcp, void* recv_klass);

sgx_status_t SGX_CDECL ocall_jvm_resolve_method_C(void** retval, int bc, void* constant, int idx);

sgx_status_t SGX_CDECL ocall_jvm_resolve_invoke_C(void** retval, void* recv, void* constant, int bidx, int bc, void* recv_klass);

sgx_status_t SGX_CDECL ocall_jvm_resolve_invoke_handle();

sgx_status_t SGX_CDECL ocall_jvm_resolve_invoke_dynamic();

sgx_status_t SGX_CDECL ocall_jvm_ldc(void** retval, bool wide, void* p, int index);

sgx_status_t SGX_CDECL ocall_jvm_resolve_ldc(void** retval, void* p, int index, int byte);

sgx_status_t SGX_CDECL ocall_jvm_resolve_get_put(int* retval, int byte, void* mh, int idx, void* bcp);

sgx_status_t SGX_CDECL ocall_jvm_resolve_get_put_klass(void** retval, int byte, void* mh, int idx, void* bcp);

sgx_status_t SGX_CDECL ocall_klass_type_array(void** retval);

sgx_status_t SGX_CDECL ocall_klass_type(void** retval);

sgx_status_t SGX_CDECL ocall_obj_array_klass_get(void** retval, void* pool, int index);

sgx_status_t SGX_CDECL ocall_klass_resolve_or_fail(void** retval, char* name);

sgx_status_t SGX_CDECL ocall_klass_find_or_null(void** retval, char* name);

sgx_status_t SGX_CDECL ocall_klass_get(void** retval, void* pool, int index);

sgx_status_t SGX_CDECL ocall_klass_compute_oopmap(void* klass, void* m, int bci);

sgx_status_t SGX_CDECL ocall_array_klass(void** retval, void* klass, int rank, int get_all);

sgx_status_t SGX_CDECL ocall_multi_array_klass_get(void** retval, void* pool, int index);

sgx_status_t SGX_CDECL ocall_jvm_pre_native(void* method, int resolve);

sgx_status_t SGX_CDECL ocall_jvm_malloc(void** retval, int size);

#ifdef  __cplusplus
}
#endif

#endif //LINUX_SGX_SIM_enclave/share/vm/enclave/sc/securecompiler_T_H
