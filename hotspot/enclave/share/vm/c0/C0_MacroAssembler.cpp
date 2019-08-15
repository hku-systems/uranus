//
// Created by jianyu on 11/17/18.
//

#include <precompiled.hpp>
#include "C0_MacroAssembler.hpp"
#include "c0_CodeStubs.hpp"

void C0_MacroAssembler::jump_to_compiled(Register method, address entry, bool force_compile, PatchingStub* &stub) {
    lea(r13, Address(rsp, 0));
    movptr(Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize), r13);
    if (entry == (address)-1) {
      movptr(rax, Address(method, Method::enclave_native_function_offset()));
      call(rax);
    } else if (entry != NULL) {
         emit_int8((unsigned char)0xE8);
         intptr_t disp = entry - (pc() + sizeof(int32_t));
         emit_int32(disp);
    } else {
        // jump to the compile stub, if it is not a interface, remove the patch after
        stub = new PatchingStub(this, PatchingStub::compile_method_id, force_compile);
        stub->install();
        movptr(rax, Address(method, Method::enclave_native_function_offset()));
        call(rax);
    }
}

void C0_MacroAssembler::addptr(Register dst, int32_t imm32)  {
    if (dst == rsp && start_expression) {
        current_entry->clear_bit_prev(imm32 / Interpreter::stackElementSize);
    }
    InterpreterMacroAssembler::addptr(dst, imm32);
}

void C0_MacroAssembler::initialize_header(Register obj, Register klass, Register len, Register t1, Register t2) {
    assert_different_registers(obj, klass, len);
    if (UseBiasedLocking && !len->is_valid()) {
        assert_different_registers(obj, klass, len, t1, t2);
        movptr(t1, Address(klass, Klass::prototype_header_offset()));
        movptr(Address(obj, oopDesc::mark_offset_in_bytes()), t1);
    } else {
        // This assumes that all prototype bits fit in an int32_t
        movptr(Address(obj, oopDesc::mark_offset_in_bytes ()), (int32_t)(intptr_t)markOopDesc::prototype());
    }
#ifdef _LP64
    if (UseCompressedClassPointers) { // Take care not to kill klass
        movptr(t1, klass);
        encode_klass_not_null(t1);
        movl(Address(obj, oopDesc::klass_offset_in_bytes()), t1);
    } else
#endif
    {
        movptr(Address(obj, oopDesc::klass_offset_in_bytes()), klass);
    }

    if (len->is_valid()) {
        movl(Address(obj, arrayOopDesc::length_offset_in_bytes()), len);
    }
#ifdef _LP64
    else if (UseCompressedClassPointers) {
        xorptr(t1, t1);
        store_klass_gap(obj, t1);
    }
#endif
}

void C0_MacroAssembler::initialize_body(Register obj, Register len_in_bytes, int hdr_size_in_bytes, Register t1) {
    Label done;
    assert(obj != len_in_bytes && obj != t1 && t1 != len_in_bytes, "registers must be different");
    assert((hdr_size_in_bytes & (BytesPerWord - 1)) == 0, "header size is not a multiple of BytesPerWord");
    Register index = len_in_bytes;
    // index is positive and ptr sized
    subptr(index, hdr_size_in_bytes);
    jcc(Assembler::zero, done);
    // initialize topmost word, divide index by 2, check if odd and test if zero
    // note: for the remaining code to work, index must be a multiple of BytesPerWord
#ifdef ASSERT
    { Label L;
    testptr(index, BytesPerWord - 1);
    jcc(Assembler::zero, L);
    stop("index is not a multiple of BytesPerWord");
    bind(L);
  }
#endif
    xorptr(t1, t1);    // use _zero reg to clear memory (shorter code)
    if (UseIncDec) {
        shrptr(index, 3);  // divide by 8/16 and set carry flag if bit 2 was set
    } else {
        shrptr(index, 2);  // use 2 instructions to avoid partial flag stall
        shrptr(index, 1);
    }
#ifndef _LP64
    // index could have been not a multiple of 8 (i.e., bit 2 was set)
  { Label even;
    // note: if index was a multiple of 8, than it cannot
    //       be 0 now otherwise it must have been 0 before
    //       => if it is even, we don't need to check for 0 again
    jcc(Assembler::carryClear, even);
    // clear topmost word (no jump needed if conditional assignment would work here)
    movptr(Address(obj, index, Address::times_8, hdr_size_in_bytes - 0*BytesPerWord), t1);
    // index could be 0 now, need to check again
    jcc(Assembler::zero, done);
    bind(even);
  }
#endif // !_LP64
    // initialize remaining object fields: rdx is a multiple of 2 now
    { Label loop;
        bind(loop);
        movptr(Address(obj, index, Address::times_8, hdr_size_in_bytes - 1*BytesPerWord), t1);
        NOT_LP64(movptr(Address(obj, index, Address::times_8, hdr_size_in_bytes - 2*BytesPerWord), t1);)
        decrement(index);
        jcc(Assembler::notZero, loop);
    }

    // done
    bind(done);
}

void C0_MacroAssembler::initialize_object(Register obj, Register klass, Register var_size_in_bytes, int con_size_in_bytes, Register t1, Register t2) {
    assert((con_size_in_bytes & MinObjAlignmentInBytesMask) == 0,
           "con_size_in_bytes is not multiple of alignment");
    const int hdr_size_in_bytes = instanceOopDesc::header_size() * HeapWordSize;

    initialize_header(obj, klass, noreg, t1, t2);

    // clear rest of allocated space
    const Register t1_zero = t1;
    const Register index = t2;
    const int threshold = 6 * BytesPerWord;   // approximate break even point for code size (see comments below)
    if (var_size_in_bytes != noreg) {
        mov(index, var_size_in_bytes);
        initialize_body(obj, index, hdr_size_in_bytes, t1_zero);
    } else if (con_size_in_bytes <= threshold) {
        // use explicit null stores
        // code size = 2 + 3*n bytes (n = number of fields to clear)
        xorptr(t1_zero, t1_zero); // use t1_zero reg to clear memory (shorter code)
        for (int i = hdr_size_in_bytes; i < con_size_in_bytes; i += BytesPerWord)
            movptr(Address(obj, i), t1_zero);
    } else if (con_size_in_bytes > hdr_size_in_bytes) {
        // use loop to null out the fields
        // code size = 16 bytes for even n (n = number of fields to clear)
        // initialize last object field first if odd number of fields
        xorptr(t1_zero, t1_zero); // use t1_zero reg to clear memory (shorter code)
        movptr(index, (con_size_in_bytes - hdr_size_in_bytes) >> 3);
        // initialize last object field if constant size is odd
        if (((con_size_in_bytes - hdr_size_in_bytes) & 4) != 0)
            movptr(Address(obj, con_size_in_bytes - (1*BytesPerWord)), t1_zero);
        // initialize remaining object fields: rdx is a multiple of 2
        { Label loop;
            bind(loop);
            movptr(Address(obj, index, Address::times_8, hdr_size_in_bytes - (1*BytesPerWord)),
                   t1_zero);
            NOT_LP64(movptr(Address(obj, index, Address::times_8, hdr_size_in_bytes - (2*BytesPerWord)),
                            t1_zero);)
            decrement(index);
            jcc(Assembler::notZero, loop);
        }
    }

    verify_oop(obj);
}

void C0_MacroAssembler::sgx_bound_check(Register obj, Label &check_fail) {
#ifdef ENCLAVE_BOUND_CHECK
#ifdef ENCLAVE_MPX
    bndcl(bnd0, obj);
    bndcu(bnd0, obj);
#else
    cmpptr(obj, r8);
    jcc(Assembler::less, check_fail);
    cmpptr(obj, r9);
    jcc(Assembler::aboveEqual, check_fail);
#endif
#endif
}
