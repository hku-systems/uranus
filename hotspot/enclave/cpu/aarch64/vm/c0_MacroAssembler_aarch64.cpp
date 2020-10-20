//
// Created by jianyu on 11/17/18.
//

#include <precompiled.hpp>
#include "c0/c0_CodeStubs.hpp"

// Zero words; len is in bytes
// Destroys all registers except addr
// len must be a nonzero multiple of wordSize
void C0_MacroAssembler::zero_memory(Register addr, Register len, Register t1) {
    assert_different_registers(addr, len, t1, rscratch1, rscratch2);

#ifdef ASSERT
    { Label L;
    tst(len, BytesPerWord - 1);
    br(Assembler::EQ, L);
    stop("len is not a multiple of BytesPerWord");
    bind(L);
  }
#endif

#ifndef PRODUCT
    block_comment("zero memory");
#endif

    Label loop;
    Label entry;

//  Algorithm:
//
//    scratch1 = cnt & 7;
//    cnt -= scratch1;
//    p += scratch1;
//    switch (scratch1) {
//      do {
//        cnt -= 8;
//          p[-8] = 0;
//        case 7:
//          p[-7] = 0;
//        case 6:
//          p[-6] = 0;
//          // ...
//        case 1:
//          p[-1] = 0;
//        case 0:
//          p += 8;
//      } while (cnt);
//    }

    const int unroll = 8; // Number of str(zr) instructions we'll unroll

    lsr(len, len, LogBytesPerWord);
    andr(rscratch1, len, unroll - 1);  // tmp1 = cnt % unroll
    sub(len, len, rscratch1);      // cnt -= unroll
    // t1 always points to the end of the region we're about to zero
    add(t1, addr, rscratch1, Assembler::LSL, LogBytesPerWord);
    adr(rscratch2, entry);
    sub(rscratch2, rscratch2, rscratch1, Assembler::LSL, 2);
    br(rscratch2);
    bind(loop);
    sub(len, len, unroll);
    for (int i = -unroll; i < 0; i++)
        str(zr, Address(t1, i * wordSize));
    bind(entry);
    add(t1, t1, unroll * wordSize);
    cbnz(len, loop);
}


void C0_MacroAssembler::jump_to_compiled(Register method, address entry, bool force_compile, PatchingStub* &stub) {

    // set sender sp
    mov(r13, sp);
    // record last_sp
    str(esp, Address(rfp, frame::interpreter_frame_last_sp_offset * wordSize));

    //call to blr
    if (entry == (address)-1) {
        ldr(r0, Address(method, Method::from_compiled_offset()));
        blr(r0);
    } else if (entry != NULL) {
        mov(r0, entry);
        blr(r0);
    } else {
        ldr(r0, Address(method, Method::from_compiled_offset()));
        blr(r0);
    }
}
/*
void C0_MacroAssembler::addptr(Register dst, int32_t imm32)  {
    if (dst == rsp && start_expression) {
        current_entry->clear_bit_prev(imm32 / Interpreter::stackElementSize);
    }
    InterpreterMacroAssembler::addptr(dst, imm32);
}
*/
void C0_MacroAssembler::initialize_header(Register obj, Register klass, Register len, Register t1, Register t2) {
    //comment out because not used in NormalCompileTask
    assert_different_registers(obj, klass, len);
    if (UseBiasedLocking && !len->is_valid()) {
        assert_different_registers(obj, klass, len, t1, t2);
        ldr(t1, Address(klass, Klass::prototype_header_offset()));
    } else {
        // This assumes that all prototype bits fit in an int32_t
        mov(t1, (int32_t)(intptr_t)markOopDesc::prototype());
    }
    str(t1, Address(obj, oopDesc::mark_offset_in_bytes()));

    if (UseCompressedClassPointers) { // Take care not to kill klass
        encode_klass_not_null(t1, klass);
        strw(t1, Address(obj, oopDesc::klass_offset_in_bytes()));
    } else {
        str(klass, Address(obj, oopDesc::klass_offset_in_bytes()));
    }

    if (len->is_valid()) {
        strw(len, Address(obj, arrayOopDesc::length_offset_in_bytes()));
    } else if (UseCompressedClassPointers) {
        store_klass_gap(obj, zr);
    }
}

void C0_MacroAssembler::initialize_body(Register obj, Register len_in_bytes, int hdr_size_in_bytes, Register t1) {
    //comment out because not used in NormalCompileTask
    Label done;
    assert(obj != len_in_bytes && obj != t1 && t1 != len_in_bytes, "registers must be different");
    assert((hdr_size_in_bytes & (BytesPerWord - 1)) == 0, "header size is not a multiple of BytesPerWord");
    Register index = len_in_bytes;
    // index is positive and ptr sized
    subs(index, index, hdr_size_in_bytes);
    br(Assembler::EQ, done);
    // note: for the remaining code to work, index must be a multiple of BytesPerWord
#ifdef ASSERT
    { Label L;
    tst(index, BytesPerWord - 1);
    br(Assembler::EQ, L);
    stop("index is not a multiple of BytesPerWord");
    bind(L);
  }
#endif

    // Preserve obj
    if (hdr_size_in_bytes)
        add(obj, obj, hdr_size_in_bytes);
    zero_memory(obj, index, t1);
    if (hdr_size_in_bytes)
        sub(obj, obj, hdr_size_in_bytes);

    // done
    bind(done);
}

void C0_MacroAssembler::initialize_object(Register obj, Register klass, Register var_size_in_bytes, int con_size_in_bytes, Register t1, Register t2) {
    //comment out because not used in NormalCompileTask
    assert((con_size_in_bytes & MinObjAlignmentInBytesMask) == 0,
           "con_size_in_bytes is not multiple of alignment");
    const int hdr_size_in_bytes = instanceOopDesc::header_size() * HeapWordSize;

    initialize_header(obj, klass, noreg, t1, t2);

    // clear rest of allocated space
    const Register index = t2;
    const int threshold = 16 * BytesPerWord;   // approximate break even point for code size (see comments below)
    if (var_size_in_bytes != noreg) {
        mov(index, var_size_in_bytes);
        initialize_body(obj, index, hdr_size_in_bytes, t1);
    } else if (con_size_in_bytes <= threshold) {
        // use explicit null stores
        int i = hdr_size_in_bytes;
        if (i < con_size_in_bytes && (con_size_in_bytes % (2 * BytesPerWord))) {
            str(zr, Address(obj, i));
            i += BytesPerWord;
        }
        for (; i < con_size_in_bytes; i += 2 * BytesPerWord)
            stp(zr, zr, Address(obj, i));
    } else if (con_size_in_bytes > hdr_size_in_bytes) {
        block_comment("zero memory");
        // use loop to null out the fields

        int words = (con_size_in_bytes - hdr_size_in_bytes) / BytesPerWord;
        mov(index,  words / 8);

        const int unroll = 8; // Number of str(zr) instructions we'll unroll
        int remainder = words % unroll;
        lea(rscratch1, Address(obj, hdr_size_in_bytes + remainder * BytesPerWord));

        Label entry_point, loop;
        b(entry_point);

        bind(loop);
        sub(index, index, 1);
        for (int i = -unroll; i < 0; i++) {
            if (-i == remainder)
                bind(entry_point);
            str(zr, Address(rscratch1, i * wordSize));
        }
        if (remainder == 0)
            bind(entry_point);
        add(rscratch1, rscratch1, unroll * wordSize);
        cbnz(index, loop);

    }

    membar(StoreStore);

    /*
    if (CURRENT_ENV->dtrace_alloc_probes()) {
        assert(obj == r0, "must be");
        far_call(RuntimeAddress(Runtime0::entry_for(Runtime0::dtrace_object_alloc_id)));
    }
     */

    verify_oop(obj);
}

void C0_MacroAssembler::sgx_bound_check(Register obj, Label &check_fail) {
#ifdef ENCLAVE_BOUND_CHECK
#ifdef ENCLAVE_MPX
    bndcl(bnd0, obj);
    bndcu(bnd0, obj);
#else
    // last 2 registers to hold argument values
    __ cmp(obj, r6);
    // less; jcc to cbz
    __ b.lt check_fail;
    __ cmp(obj, r7);
    //above equal to HS
    __ b.gt check_fail;
#endif
#endif
}
