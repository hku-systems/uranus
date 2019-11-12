//
// Created by max on 1/15/18.
//

#include "precompiled/precompiled.hpp"
#include "runtime/stubCodeGenerator.hpp"
#include "asm/assembler.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/java.hpp"
#include "PreInterpreterCallStubGenerator.h"
#include "CompilerEnclave.h"
#include "securecompiler_u.h"

pre_interpreter_stub_t PreInterpreterCallStubGenerator::pre_interpreter_stub = NULL;
ocall_interpreter_stub_t PreInterpreterCallStubGenerator::ocall_interpreter_stub = NULL;

address PreInterpreterCallStubGenerator::register_buf_rbx = (address)(new char[8]);
address PreInterpreterCallStubGenerator::register_buf_rcx = (address)(new char[8]);
address PreInterpreterCallStubGenerator::register_buf_rdx = (address)(new char[8]);

PreInterpreterCallStubGenerator::PreInterpreterCallStubGenerator(CodeBuffer *c) : StubCodeGenerator(c) {}

address PreInterpreterCallStubGenerator::generate_interpreter_entry() {

    StubCodeMark mark(this, "PreInterpreter", "interpreter_entry");
    #   define __ _masm->
    address start = __ pc();
    // get the ret address, rax is not necessary to preserve
    // get enough space for ret address and ret value from the enclave

#ifdef TARGET_ARCH_aarch64
    __ enter();
    // copy registers and rsp
    __ pusha();
    __ mov(c_rarg0, sp);
    __ mov(c_rarg1, rthread);
    __ mov(c_rarg2, rmethod);

    address jjp = (address)CompilerEnclave::call_interpreter_zero_locals;
    __ mov(c_rarg3, jjp);
    __ blr(c_rarg3);

    // restore and return
    __ leave();
    __ ret(lr);
#else

#endif
    #   undef __
    return start;

};

address PreInterpreterCallStubGenerator::generate_ocall_interpreter_entry() {
    StubCodeMark mark(this, "PreInterpreter", "interpreter_entry");
    #   define __ _masm->
    address ret = __ pc();
    address start = ret;
    return start;
};

void PreInterpreterCallStubGenerator::generate_pre_interpreter_entry() {
    ResourceMark rm;
    // Making this stub must be FIRST use of assembler

    BufferBlob* stub_blob = BufferBlob::create("get_cpu_info_stub", 128);
    if (stub_blob == NULL) {
        vm_exit_during_initialization("Unable to allocate get_cpu_info_stub");
    }
    CodeBuffer c(stub_blob);
    PreInterpreterCallStubGenerator g = PreInterpreterCallStubGenerator(&c);
    pre_interpreter_stub = CAST_TO_FN_PTR(pre_interpreter_stub_t ,
                                          g.generate_interpreter_entry());
}
