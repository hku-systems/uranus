//
// Created by Maxxie Jiang on 22/11/2018.
//

#ifndef HOTSPOT_COMPILETASK_HPP
#define HOTSPOT_COMPILETASK_HPP

#include <asm/macroAssembler.hpp>
#include <interp_masm_x86.hpp>
#include "bytecodeStream.hpp"
#include "c0_CodeStubs.hpp"


class CompileTask {
public:
    C0_MacroAssembler*  _masm;
    Method* method;
    address method_entry;
    CompileTask(Method* _method): method(_method) {}
    virtual int compile(int size) = 0;
    virtual void entry() = 0;
    void print_disassembly();
    void generate_fixed_frame(bool native_call);
    void set_compile_entry() {
        method->set_enclave_native_function(method_entry);
    }
    void generate_gc_check(Label &l);
    void generate_gc_barrier(Label &l);
};


#endif //HOTSPOT_COMPILETASK_HPP
