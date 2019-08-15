//
// Created by jianyu on 11/24/18.
//

#ifndef HOTSPOT_C0MAPOOPENTRY_HPP
#define HOTSPOT_C0MAPOOPENTRY_HPP

#include <enclave/sc/EnclaveDebug.h>
#include <sys/types.h>
#include <utilities/globalDefinitions.hpp>
#include <oops/method.hpp>

class C0MapOopEntry {
public:
    // use mask to indicate oop of a variable
    uintptr_t *_mask;
    int n_mask;
    int entries;
    int capacity;
    enum {
        bits_per_entry = 1,
    };

    enum {
        oop_bit_number = 1,
        primitive_bit_number = 0
    };

    C0MapOopEntry(Method* m);
    C0MapOopEntry() {}
    void push(TosState s);
    void pop(TosState s);
    void set_bit_next(uint bit, int step = 1);
    void set_bit(int idx, uint bit);
    void clear_bit_prev(int step = 1);
    void expand();
    C0MapOopEntry* clone();
};


#endif //HOTSPOT_C0MAPOOPENTRY_HPP
