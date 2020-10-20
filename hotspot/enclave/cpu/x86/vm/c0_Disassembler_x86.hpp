//
// Created by jianyu on 11/15/18.
//

#ifndef HOTSPOT_DISASSEMBLER_HPP
#define HOTSPOT_DISASSEMBLER_HPP

#include "udis/udis86.h"

class Disassembler {
public:

    ud_t *ud_obj;
    char *start_addr, *end_addr;

    Disassembler(void* addr, int len);
    void print_asm();
    ~Disassembler() { 
        // delete ud_obj;
    }
};


#endif //HOTSPOT_DISASSEMBLER_HPP
