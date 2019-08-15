//
// Created by jianyu on 11/15/18.
//

#include "StubAssembler.hpp"
#include "c0_Runtime.hpp"
#include "C0_MacroAssembler.hpp"

StubAssembler::StubAssembler(CodeBuffer* code, const char * name, Runtime0::StubID stub_id) : C0_MacroAssembler(code, NULL) {
    _name = name;
    _must_gc_arguments = false;
//    _frame_size = no_frame_size;
    _num_rt_args = 0;
    _stub_id = stub_id;
}


void StubAssembler::set_num_rt_args(int args) {
    if (_num_rt_args == 0) {
        _num_rt_args = args;
    }
    assert(_num_rt_args == args, "can't change the number of args");
}