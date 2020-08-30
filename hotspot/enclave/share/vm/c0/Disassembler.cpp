//
// Created by jianyu on 11/15/18.
//

#include "precompiled.hpp"
#include <stdio.h>
//#include <cstdint>
#include <cstdio>
#include "Disassembler.hpp"
#include <stdint.h>

#ifdef TARGET_ARCH_x86
#include <udis/udis86.h>
#endif
#ifdef TARGET_ARCH_aarch64
#include <udis/disasm.h>
#endif

Disassembler::Disassembler(void *addr, int len){
    /*
    ud_obj = new ud_t();
    ud_init(ud_obj);
    ud_set_mode(ud_obj, 64);
    ud_set_pc(ud_obj, (uint64_t)addr);
    ud_set_input_buffer(ud_obj, (unsigned char*)addr, len);
    ud_set_syntax(ud_obj, UD_SYN_INTEL);
     */
    start_addr = addr;
    end_addr = addr + len;
}

void Disassembler::print_asm() {
    /*
    while (ud_disassemble(ud_obj)) {
        //printf("\t%lx: %s\n", ud_insn_ptr(ud_obj), ud_insn_asm(ud_obj));
    }
     */
    char* cur_addr = start_addr;
    char *str_buf = new char[10240];
    memset(str_buf, 0, 10240);
    while (1) {
        cur_addr = disasm(cur_addr, str_buf);
        printf("\t%lx:%s\n", cur_addr, str_buf);
        if (cur_addr >= end_addr)
            break;
    }
}
