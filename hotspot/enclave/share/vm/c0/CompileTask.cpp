//
// Created by Maxxie Jiang on 22/11/2018.
//

#include <precompiled.hpp>
#include "CompileTask.hpp"
#include "Disassembler.hpp"
#include "c0_Runtime.hpp"

#define __ _masm->

void CompileTask::generate_gc_check(Label &gc_barrier) {
    Label after_gc_barrier;
    __ movptr(rdx, RuntimeAddress((address)&EnclaveGC::is_gc_waiting));
    __ testb(rdx, 0);
    __ jcc(Assembler::zero, after_gc_barrier);
    generate_gc_barrier(gc_barrier);
    __ bind(after_gc_barrier);
}

void CompileTask::generate_gc_barrier(Label &gc_barrier_entry) {
    __ bind(gc_barrier_entry);
    address gc_barrier = Runtime0::entry_for(Runtime0::gc_barrier_id);
    __ call(RuntimeAddress(gc_barrier));
}

void CompileTask::print_disassembly() {
    Disassembler disassembler(method_entry, _masm->pc() - method_entry);
    disassembler.print_asm();
    printf("fin %s %s\n", method->klass_name()->as_C_string(), method->name()->as_C_string());
}

void CompileTask::generate_fixed_frame(bool native_call) {
    // initialize fixed part of activation frame
    __ push(rax);        // save return address
    __ enter();          // save old & set new rbp
    __ push(r13);        // set sender sp
    __ push((int)NULL_WORD); // leave last_sp as null
//    __ movptr(r13, Address(rbx, Method::const_offset()));      // get ConstMethod*
//    __ lea(r13, Address(r13, ConstMethod::codes_offset())); // get codebase
//    __ movptr(r13, (intptr_t)method->code_base());
    __ push(rbx);        // save Method*
    if (ProfileInterpreter) {

    } else {
        __ push(0);
    }

//    __ movptr(rdx, Address(rbx, Method::const_offset()));
//    __ movptr(rdx, Address(rdx, ConstMethod::constants_offset()));
//    __ movptr(rdx, Address(rdx, ConstantPool::cache_offset_in_bytes()));
//    __ movptr(rdx, (intptr_t)method->constants());
    __ push(rdx); // set constant pool cache
    __ push(r14); // set locals pointer
    if (native_call) {
        __ push(0); // no bcp
    } else {
        __ push(r13); // set bcp
    }
    __ push(0); // reserve word for pointer to expression stack bottom
    __ movptr(Address(rsp, 0), rsp); // set expression stack bottom
}

#undef __