
#ifndef ENCLAVE_HEADER_OCALL
#define ENCLAVE_HEADER_OCALL
#include "securecompiler_t.h"
#include "EnclaveDebug.h"

// allocate memory outside enclave
void* JVM_ENTRY_omalloc(int size OCallCallerDefs);

void JVM_ENTRY_resolve_invoke(void* thread,int bytecode OCallCallerDefs);

void* JVM_ENTRY_resolve_invoke_C(void* recv, void* constant, int bidx, int OCallCallerDefs);

void* JVM_ENTRY_resolve_method_C(int bc, void* constant, int idx OCallCallerDefs);

void JVM_ENTRY_resolve_invoke_dynamic(void* thread OCallCallerDefs);

void JVM_ENTRY_resolve_invoke_handle(void* thread OCallCallerDefs);

void* JVM_ENTRY_ldc(void* thread, bool, void*, int OCallCallerDefs);

void* JVM_ENTRY_resolve_ldc(void* thread, void*, int, int OCallCallerDefs);

void KLASS_compute_oopmap(void*, void*, int OCallCallerDefs);

void* KLASS_resolve_or_fail(const char* name OCallCallerDefs);

void* KLASS_find_or_null(const char* name OCallCallerDefs);

void* KLASS_get_type_klass(void* dummy = NULL OCallCallerDefs);

void* KLASS_get_type_array_klass(void* dummy = NULL OCallCallerDefs);

void* KLASS_array_klass(void*, int, int OCallCallerDefs);

void* KLASS_get_obj_array_klass(void* pool, int index OCallCallerDefs);

void* KLASS_get_multi_array_klass(void* pool, int index OCallCallerDefs);

void* JVM_ENTRY_resolve_klass(void* thread, void* pool, int index OCallCallerDefs);

void* JVM_ENTRY_quick_cc(void* thread, void* pool, int index OCallCallerDefs);

void JVM_ENTRY_resolve_get_put(void* thread, int bytecode OCallCallerDefs);

int JVM_ENTRY_resolve_get_put_C(void* thread, void* constant, int index, int bytecode OCallCallerDefs);

void* Klass_resolve_get_put(void* thread, void* method, int index, int bytecode OCallCallerDefs);

void JVM_ENTRY_pre_native(void* thread, void* method, bool resolve OCallCallerDefs);

#endif
