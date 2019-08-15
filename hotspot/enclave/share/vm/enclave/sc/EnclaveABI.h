//
// Created by jianyu on 6/6/18.
//

#ifndef HOTSPOT_ENCLAVEABI_H
#define HOTSPOT_ENCLAVEABI_H


#include "asm/codeBuffer.hpp"
#include "runtime/stubCodeGenerator.hpp"

typedef void* (*interpreter_stub_t)(void*, void*, JavaThread*, address);
typedef address (*get_exception_stub_t)();

class EnclaveABI: public StubCodeGenerator {
public:
    EnclaveABI(CodeBuffer *c);

    static void init();
    static address interpreter_ret;
    static address exception_ret;
    static address do_ocall;

    static interpreter_stub_t do_ecall;

    static get_exception_stub_t get_exception;

    address generate_interpreter_entry();

    address generate_ocall_entry();

    static void* copy_parameter(void* r14, int size);

    static void* call_interpreter(void* r14, int size, void* method, void* thread, void* sender);

    address generate_heap_alloc();
};


#endif //HOTSPOT_ENCLAVEABI_H
