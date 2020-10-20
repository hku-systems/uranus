//
// Created by jianyu on 11/15/18.
//

#ifndef HOTSPOT_DISASSEMBLER_HPP
#define HOTSPOT_DISASSEMBLER_HPP

class Disassembler {
public:
    char *start_addr, *end_addr;

    Disassembler(void* addr, int len);
    void print_asm();
    ~Disassembler() {}
};


#endif //HOTSPOT_DISASSEMBLER_HPP
