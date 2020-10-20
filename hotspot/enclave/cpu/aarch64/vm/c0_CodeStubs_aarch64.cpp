//
// Created by Maxxie Jiang on 14/11/2018.
//

#include <precompiled.hpp>
#include <nativeInst_aarch64.hpp>
#include "c0/c0_CodeStubs.hpp"
#include "c0/c0_Runtime.hpp"

#define __ _masm->

int PatchingStub::_patch_info_offset = -NativeGeneralJump::instruction_size;

void PatchingStub::install() {
    while (_masm->pc() - _pc_start < NativeGeneralJump::instruction_size) {
      _masm->nop();
    }
    _bytes_to_copy = __ pc() - _pc_start;
    
    if (_bytes_to_copy % 4 != 0) {
        ShouldNotReachHere();
    }

    __ bind(_patch_site_continuation);
    if (_id == PatchingStub::access_field_id) {
        // embed a fixed offset to handle long patches which need to be offset by a word.
        // the patching code will just add the field offset field to this offset so
        // that we can refernce either the high or low word of a double word field.
//         NativeMemOffset* n_move = nativeMemOffset_at(_pc_start);
//         if (n_move->offset() != 0xff) {
//             ShouldNotReachHere();
//         }
    } else if (_id == load_klass_id || _id == load_mirror_id || _id == load_appendix_id || _id == load_method_id || _id == compile_method_id) {
        assert(_obj != noreg, "must have register object for load_klass/load_mirror");
    } else {
        ShouldNotReachHere();
    }

}

void PatchingStub::emit() {

    if (_is_emmit) printf("is already emit\n");
    _is_emmit = true;

    Label call_patch;

    // static field accesses have special semantics while the class
    // initializer is being run so we emit a test which can be used to
    // check that this code is being executed by the initializing
    // thread.
    address being_initialized_entry = __ pc();


    // make a copy the code which is going to be patched.
    for (int i = 0; i < _bytes_to_copy; i++) {
        address ptr = (address)(_pc_start + i);
        int a_byte = (*ptr) & 0xFF;
        __ emit_int8(a_byte);
    }

    // if (_bytes_to_copy * 2 < NativeGeneralJump::instruction_size) {
    //     for (int i = 0;i < NativeGeneralJump::instruction_size - _bytes_to_copy * 2;i++) {
    //         __ nop();
    //     }
    // }

    address end_of_patch = __ pc();
    int bytes_to_skip = 0;
    if (_id == load_mirror_id) {
        // TODO: test init
    }

    // Now emit the patch record telling the runtime how to find the
    // pieces of the patch.  We only need 3 bytes but for readability of
    // the disassembly we make the data look like a movl reg, imm32,
    // which requires 5 bytes
    int sizeof_patch_record = 5;
    bytes_to_skip += sizeof_patch_record;

    // emit the offsets needed to find the code to patch
    int being_initialized_entry_offset = __ pc() - being_initialized_entry + sizeof_patch_record;

    // __ emit_int8((unsigned char)0xB8);
    // __ emit_int8(0);
    // __ emit_int8(being_initialized_entry_offset);
    // __ emit_int8(bytes_to_skip);
    // __ emit_int8(_bytes_to_copy);

    __ nop();
    address st = __ pc();
    __ nop();
    __ nop();
    __ nop();
    __ nop();

    NativeGeneralJump::insert_unconditional((address)st, 
        (((_bytes_to_copy & 0xff) << 16) | (_bci & 0xffff)));
    __ nop();
    address entry = __ pc();
    // printf("start %lx, copy %lx, init %lx, st %lx, entry %lx, cur %lx %d\n", _pc_start, being_initialized_entry, end_of_patch, st, entry, __ pc(), _bytes_to_copy);
    address target = NULL;

    switch (_id) {
        case access_field_id:       target = Runtime0::entry_for(Runtime0::access_field_patching_id);    break;
        case load_klass_id:         target = Runtime0::entry_for(Runtime0::load_klass_patching_id);     break;
        case load_mirror_id:        target = Runtime0::entry_for(Runtime0::load_mirror_patching_id);    break;
        case load_appendix_id:      target = Runtime0::entry_for(Runtime0::load_appendix_patching_id);  break;
        case load_method_id:        target = Runtime0::entry_for(Runtime0::load_method_patching_id);    break;
        case compile_method_id:     target = Runtime0::entry_for(Runtime0::compile_method_patching_id);    break;
        default: ShouldNotReachHere();
    }

    //call to below
    //__ call(RuntimeAddress(target));
    __ lea(rscratch1, RuntimeAddress(target));
    __ blr(rscratch1);
    if (_id == compile_method_id) {
        //jmp to b
        __ b(_patch_site_continuation);
    } else {
        //jmp to b
        __ b(_patch_site_entry);
    }
    NativeGeneralJump::insert_unconditional((address)_pc_start, entry);
}

#undef __
