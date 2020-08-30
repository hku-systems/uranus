


#include "em_ocall.h"
#include <stdio.h>
#include <malloc.h>

#define CACHE_ENTRY(method, bcp) method->constants()->cache()->entry_at(Bytes::get_native_u2((address)bcp + 1))
#define CODE_AT(method, bcp) Bytecodes::code_at(method, bcp)
#define IS_RESOLVE(method, bcp) CACHE_ENTRY(method, bcp)->is_resolved(CODE_AT(method, (address)bcp))

#ifdef DB_OCALL
#define OCALL_TRACE printf("OCALL %s\n", __FUNCTION__)
#else
#define OCALL_TRACE
#endif

#define CHECK_RETURN_(ret) 

#define CHECK_RETURN

#define OCALL_ENTRY(ret, header) ret header {

#define OCALL_END }

OCALL_ENTRY(void*, ocall_interpreter(void* r14, int size, void* method, void* thread, void* sender))

OCALL_END

OCALL_ENTRY(void*,ocall_jvm_malloc(int size))
    return malloc(size);
OCALL_END

OCALL_ENTRY(int, ocall_jvm_resolve_get_put(int byte, void* mh, int idx, void* bcp))

OCALL_END

OCALL_ENTRY(void*, ocall_jvm_resolve_get_put_klass(int byte, void* mh, int idx, void* bcp))

OCALL_END

OCALL_ENTRY(void, ocall_jvm_resolve_invoke_handle())

OCALL_END

void ocall_jvm_resolve_invoke_dynamic() {

OCALL_END

OCALL_ENTRY(void, ocall_jvm_resolve_invoke(int byte, void* mh, int bci, void* recv, int idx, void* bcp, void* recv_klass))

OCALL_END

OCALL_ENTRY(void*, ocall_jvm_resolve_ldc(void*p, int index, int byte))

OCALL_END

OCALL_ENTRY(void*, ocall_jvm_ldc(bool wide, void* p, int index))

OCALL_END

OCALL_ENTRY(void*, ocall_klass_type_array())

OCALL_END

OCALL_ENTRY(void*, ocall_klass_type())

OCALL_END

OCALL_ENTRY(void*, ocall_klass_get(void* p, int index))

OCALL_END

OCALL_ENTRY(void*, ocall_obj_array_klass_get(void*p, int index))

OCALL_END

OCALL_ENTRY(void*, ocall_multi_array_klass_get(void* p, int index))

OCALL_END

OCALL_ENTRY(void, ocall_jvm_pre_native(void* method, int resolve))

OCALL_END

OCALL_ENTRY(void*, ocall_array_klass(void* klass_v, int rank, int get_all))

OCALL_END

OCALL_ENTRY(void*, ocall_klass_resolve_or_fail(const char* name))

OCALL_END

OCALL_ENTRY(void*, ocall_klass_find_or_null(const char* name))

OCALL_END

OCALL_ENTRY(void, ocall_klass_compute_oopmap(void* k, void* m, int bci))

OCALL_END

OCALL_ENTRY(void*, ocall_jvm_resolve_method_C(int bc, void* constant, int idx))

OCALL_END

OCALL_ENTRY(void*, ocall_jvm_resolve_invoke_C(void* recv, void* constants, int bidx, int bc, void* recv_klass))

OCALL_END

OCALL_ENTRY(void*, ocall_classfile_buffer(const char* name, int* size))
    int len = 0, r = 0;
    void *buffer = NULL;
    
    printf("opening file %s\n", name);

    FILE* f = fopen(name, "rb");

    if (f == NULL) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);

    len = ftell(f);

    // reset
    rewind(f);

    buffer = (void*) malloc(len);
    r = fread(buffer, sizeof(char), len, f);
    if (r <= 0) {
        free(buffer);
        return NULL;
    }
    *size = len;
    return buffer;
OCALL_END