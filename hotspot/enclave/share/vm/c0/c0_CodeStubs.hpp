//
// Created by Maxxie Jiang on 14/11/2018.
//

#ifndef HOTSPOT_C0_CODESTUBS_H
#define HOTSPOT_C0_CODESTUBS_H


#include <interp_masm_x86.hpp>
#include "C0_MacroAssembler.hpp"

/*
 * Structure for patch access field / klass / method
 *
 * _patch_site_entry (_pc_start):   mov rdx, 0 -- NULL stub move operations
 *                                  -> override with a jmp to call_site
 *                                  -> override in patch to jmp to patch start
 * being_initialized_entry(install):[ duplicate mov here ] (_patch_site_continuation)
 * end_of_patch (skip_start):       { other stuff for load_mirror (MP) }
 *                                  mov reg, imm32
 *        (dummy, dymmy, being_initialized_entry_offset, bytes_to_skip, _bytes_to_copy)
 * init_entry_offset (callsite):    call runtime()
 *                                  jmp _patch_site_entry
 *        bytes_to_copy = install - _pc_start
 *
 *
 * */

class CodeStub {
public:
    C0_MacroAssembler *_masm;
    CodeStub(C0_MacroAssembler *masm): _masm(masm) {}
    virtual void emit() = 0;
};

class PatchingStub: public CodeStub {
private:

public:
    enum PatchID {
        access_field_id,
        load_klass_id,
        load_mirror_id,
        load_appendix_id,
        load_method_id,
        compile_method_id
    };

    static int _patch_info_offset;

    PatchID       _id;
    address       _pc_start;
    int           _bytes_to_copy;
    Label         _patch_site_entry;
    Label         _patch_site_continuation;
    int           _bci;
    int           _is_emmit;

    PatchingStub(C0_MacroAssembler *masm, PatchID id, int bci): CodeStub(masm) {
        _pc_start = masm->pc();
        _bci = bci;
        _id = id;
        _is_emmit = false;
        masm->bind(_patch_site_entry);
    }
    ~PatchingStub() {}
    void emit();
    void install();
};


#endif //HOTSPOT_C0_CODESTUBS_H
