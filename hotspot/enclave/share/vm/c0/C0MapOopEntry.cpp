//
// Created by jianyu on 11/24/18.
//

#include <precompiled.hpp>
#include "C0MapOopEntry.hpp"

C0MapOopEntry::C0MapOopEntry(Method *m) {
    _mask = new uintptr_t[1];
    n_mask = 1;
    entries = 0;
    capacity = 1;
    if (!m->is_static()) {
        push(atos);
    }

    SignatureStream ss(m->signature());
    for (;;ss.next()) {
        if (ss.at_return_type()) {
            break;
        }
        switch (ss.type()) {
            case T_ARRAY:
            case T_OBJECT:  push(atos); break;
            case T_LONG:
            case T_DOUBLE:  push(ltos); break;
            case T_VOID:    push(vtos); break;
            default:        push(itos); break;
        }
    }
    for (int i = 0;i < m->max_locals() - m->size_of_parameters();i++) {
        push(itos);
    }
}

void C0MapOopEntry::push(TosState s) {
    switch (s) {
        case atos:  set_bit_next(oop_bit_number);           break;
        case ltos:  set_bit_next(primitive_bit_number, 2);  break;
        case itos:  set_bit_next(primitive_bit_number);     break;
        default:                                            break;
    }
}


void C0MapOopEntry::pop(TosState s) {
    switch (s) {
        case itos:
        case atos:  clear_bit_prev(1);  break;
        case ltos:  clear_bit_prev(2);  break;
        default:                        break;
    }
}


void C0MapOopEntry::set_bit_next(uint bit, int step) {
    if (entries > 63) {
        // expand
        expand();
    }
    _mask[n_mask - 1] |= (bit << entries);
    entries += step;
}

void C0MapOopEntry::clear_bit_prev(int step) {
    entries -= step;
    if (entries < 0) {
        n_mask -= 1;
        entries = 63;
    }
    for (int i = 0;i < step;i++)
        _mask[n_mask - 1] &= ~(1 << (entries + i));
}

void C0MapOopEntry::expand() {
    if (capacity >= n_mask + 1) {
        n_mask += 1;
        entries = 0;
        return;
    }
    uintptr_t *new_mask = new uintptr_t[n_mask + 1];
    capacity += 1;
    for (int i = 0;i < n_mask;i++) {
        new_mask[i] = _mask[i];
    }
    n_mask += 1;
    entries = 0;
}

C0MapOopEntry* C0MapOopEntry::clone() {
    C0MapOopEntry* clone_entry =  new C0MapOopEntry();
    clone_entry->_mask = new uintptr_t[n_mask];
    for (int i = 0;i < n_mask;i++) {
        clone_entry->_mask[i] = _mask[i];
    }
    clone_entry->n_mask = n_mask;
    clone_entry->capacity = n_mask;
    clone_entry->entries = entries;
}

void C0MapOopEntry::set_bit(int idx, uint bit) {
    int mask_idx = idx / 64;
    int sub_idx = idx % 64;
    _mask[mask_idx] |= (bit << sub_idx);
}