//
// Created by jianyu on 11/15/18.
//

#ifndef HOTSPOT_STUBASSEMBLER_HPP
#define HOTSPOT_STUBASSEMBLER_HPP

#include <precompiled.hpp>
#include <interp_masm_x86.hpp>
#include "c0_Runtime.hpp"
#include "C0_MacroAssembler.hpp"

class StubAssembler: public C0_MacroAssembler {
private:
    const char* _name;
    bool        _must_gc_arguments;
    int         _frame_size;
    int         _num_rt_args;
    int         _stub_id;

public:
    // creation
    StubAssembler(CodeBuffer* code, const char * name, Runtime0::StubID stub_id);
//    void set_info(const char* name, bool must_gc_arguments);

//    void set_frame_size(int size);
    void set_num_rt_args(int args);

    // accessors
    const char* name() const                       { return _name; }
    bool  must_gc_arguments() const                { return _must_gc_arguments; }
    int frame_size() const                         { return _frame_size; }
    int num_rt_args() const                        { return _num_rt_args; }
    int stub_id() const                            { return _stub_id; }

    // runtime calls (return offset of call to be used by GC map)
    int call_RT(Register oop_result1, Register metadata_result, address entry, int args_size = 0);
    int call_RT(Register oop_result1, Register metadata_result, address entry, Register arg1);
    int call_RT(Register oop_result1, Register metadata_result, address entry, Register arg1, Register arg2);
    int call_RT(Register oop_result1, Register metadata_result, address entry, Register arg1, Register arg2, Register arg3);
};


#endif //HOTSPOT_STUBASSEMBLER_HPP
