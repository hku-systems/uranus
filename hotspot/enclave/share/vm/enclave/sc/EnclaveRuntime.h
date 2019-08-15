//
// Created by jianyu on 6/6/18.
//

#ifndef HOTSPOT_ENCLAVERUNTIME_H
#define HOTSPOT_ENCLAVERUNTIME_H

// this class store all runtime method for enclaves

#include <sgx_trts_exception.h>
#include "oops/method.hpp"
#include <map>
#include <string>
#include <sgx_spinlock.h>
#include <c0/JCompiler.hpp>

enum _debug_bit {
    debug_gc = 1,
    debug_ecall = 2,
    debug_warn = 4
};

class EnclaveRuntime {
public:
#ifdef ENCLAVE_BOUND_CHECK
#ifdef ENCLAVE_MPX
    static uintptr_t bounds[2];
#endif
#endif
    static int debug_bit;
    static int ecall_count;
    static int ocall_count;
    static volatile int in_enclave_thread_count;
    static sgx_spinlock_t count_mutex;
    static std::map<std::string, Klass*> *klass_map;
    static JCompiler *compiler;
    static bool is_init;
    static std::list<JavaThread*> threads;

    static void* init(void* cpuid, void** heap_top, void** heap_bottom, void** klass_list, int debug_bit);
    static void* call_interpreter(void* rbx_buf, Method* m);
    static void* call_compiler(void* rbx_buf, Method* m);
    static void* do_ecall_comp(void *rbx_buf, void* m, int* has_exception);
    static void* compile_method(Method* method);
    static int signal_handler(sgx_exception_info_t *info);
    static Klass* get_klass_or_null(Symbol* s) {
        return NULL;
    }
    static void insert_klass_map(Symbol*, Klass* k) {
    }
};


#endif //HOTSPOT_ENCLAVERUNTIME_H
