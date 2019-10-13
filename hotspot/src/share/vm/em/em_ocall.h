
#ifndef ENCLAVE_EM_OCALL_HEADER
#define ENCLAVE_EM_OCALL_HEADER

#if defined(__cplusplus)
extern "C" {
#endif
void* ocall_interpreter(void* r14, int size, void* method, void* thread, void* sender);
void* ocall_jvm_malloc(int size);
void ocall_jvm_resolve_invoke(int byte, void* mh, int bci, void* recv, int idx, void* bcp, void* recv_klass);
void* ocall_jvm_resolve_method_C(int bc, void* constant, int idx);
void* ocall_jvm_resolve_invoke_C(void* recv, void* constant, int bidx, int bc, void* recv_klass);
void ocall_jvm_resolve_invoke_handle();
void ocall_jvm_resolve_invoke_dynamic();
void* ocall_jvm_ldc(bool wide, void* p, int index);
void* ocall_jvm_resolve_ldc(void*p, int index, int byte);
int ocall_jvm_resolve_get_put(int byte, void* mh, int idx, void* bcp);
void* ocall_jvm_resolve_get_put_klass(int byte, void* mh, int idx, void* bcp);
void* ocall_klass_type_array();
void* ocall_klass_type();
void* ocall_klass_get(void*, int);
void* ocall_obj_array_klass_get(void*p, int index);
void* ocall_array_klass(void* klass, int rank, int get_all);
void* ocall_multi_array_klass_get(void* pool, int index);
void ocall_jvm_pre_native(void*, int);
void* ocall_klass_resolve_or_fail(const char*);
void* ocall_klass_find_or_null(const char*);
void ocall_klass_compute_oopmap(void*, void*, int);
#if defined(__cplusplus)
}
#endif

#endif
