//
// Created by jianyu on 11/15/18.
//

#include <stdio.h>
//#include <cstdint>
#include <cstdio>
#include "Disassembler.hpp"

Disassembler::Disassembler(void *addr, int len){
    ud_obj = new ud_t();
    ud_init(ud_obj);
    ud_set_mode(ud_obj, 64);
    ud_set_pc(ud_obj, (uint64_t)addr);
    ud_set_input_buffer(ud_obj, (unsigned char*)addr, len);
    ud_set_syntax(ud_obj, UD_SYN_INTEL);
}

void Disassembler::print_asm() {
    while (ud_disassemble(ud_obj)) {
        //printf("\t%lx: %s\n", ud_insn_ptr(ud_obj), ud_insn_asm(ud_obj));
    }
}
