//
// Created by jianyu on 11/15/18.
//

#include "precompiled.hpp"
#include <stdio.h>
#include <cstdio>
#include <stdint.h>

#include "c0_Disassembler_aarch64.hpp"
#include <udis/disasm.h>

Disassembler::Disassembler(void *addr, int len){
    start_addr = addr;
    end_addr = addr + len;
}

void Disassembler::print_asm() {
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
