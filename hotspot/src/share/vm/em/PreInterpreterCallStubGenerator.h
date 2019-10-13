//
// Created by max on 1/15/18.
//

#ifndef HOTSPOT_PREINTERPRETERCALLSTUBGENERATOR_H
#define HOTSPOT_PREINTERPRETERCALLSTUBGENERATOR_H

#include "precompiled.hpp"
#include "asm/assembler.hpp"
#include "runtime/stubCodeGenerator.hpp"


typedef void (*pre_interpreter_stub_t)();
typedef void* (*ocall_interpreter_stub_t)(void* r14, int size, void* method, void* thread, void* sender);

class PreInterpreterCallStubGenerator: public StubCodeGenerator {
public:

    static address register_buf_rbx;
    static address register_buf_rcx;
    static address register_buf_rdx;

    static pre_interpreter_stub_t pre_interpreter_stub;

    static ocall_interpreter_stub_t ocall_interpreter_stub;

    static void generate_pre_interpreter_entry();

    PreInterpreterCallStubGenerator(CodeBuffer *c);

    address generate_interpreter_entry();
    address generate_ocall_interpreter_entry();
};




#endif //HOTSPOT_PREINTERPRETERCALLSTUBGENERATOR_H
