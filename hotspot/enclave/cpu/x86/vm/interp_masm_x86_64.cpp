/*
 * Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "interp_masm_x86.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "oops/arrayOop.hpp"
#include "oops/markOop.hpp"
#include "oops/method.hpp"
#include "runtime/basicLock.hpp"
#include "runtime/thread.inline.hpp"


// Implementation of InterpreterMacroAssembler
#ifndef CC_INTERP

void InterpreterMacroAssembler::call_VM_leaf_base(address entry_point,
                                                  int number_of_arguments) {
  // interpreter specific
  //
  // Note: No need to save/restore bcp & locals (r13 & r14) pointer
  //       since these are callee saved registers and no blocking/
  //       GC can happen in leaf calls.
  // Further Note: DO NOT save/restore bcp/locals. If a caller has
  // already saved them so that it can use esi/edi as temporaries
  // then a save/restore here will DESTROY the copy the caller
  // saved! There used to be a save_bcp() that only happened in
  // the ASSERT path (no restore_bcp). Which caused bizarre failures
  // when jvm built with ASSERTs.
  // super call
  MacroAssembler::call_VM_leaf_base(entry_point, number_of_arguments);
  // interpreter specific
  // Used to ASSERT that r13/r14 were equal to frame's bcp/locals
  // but since they may not have been saved (and we don't want to
  // save thme here (see note above) the assert is invalid.
}

void InterpreterMacroAssembler::call_VM_base(Register oop_result,
                                             Register java_thread,
                                             Register last_java_sp,
                                             address  entry_point,
                                             int      number_of_arguments,
                                             bool     check_exceptions) {
  // interpreter specific
  //
  // Note: Could avoid restoring locals ptr (callee saved) - however doesn't
  //       really make a difference for these runtime calls, since they are
  //       slow anyway. Btw., bcp must be saved/restored since it may change
  //       due to GC.
  // assert(java_thread == noreg , "not expecting a precomputed java thread");
  save_bcp();
#ifdef ASSERT
  {
    Label L;
    cmpptr(Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize), (int32_t)NULL_WORD);
    jcc(Assembler::equal, L);
    stop("InterpreterMacroAssembler::call_VM_leaf_base:"
         " last_sp != NULL");
    bind(L);
  }
#endif /* ASSERT */
  // super call
  MacroAssembler::call_VM_base(oop_result, noreg, last_java_sp,
                               entry_point, number_of_arguments,
                               check_exceptions);
  // interpreter specific
  restore_bcp();
  restore_locals();
}


void InterpreterMacroAssembler::check_and_handle_popframe(Register java_thread) {
  // TODO: what is this?
}


void InterpreterMacroAssembler::load_earlyret_value(TosState state) {
  //
}


void InterpreterMacroAssembler::check_and_handle_earlyret(Register java_thread) {
//
}


void InterpreterMacroAssembler::get_unsigned_2_byte_index_at_bcp(
  Register reg,
  int bcp_offset) {
  assert(bcp_offset >= 0, "bcp is still pointing to start of bytecode");
  load_unsigned_short(reg, Address(r13, bcp_offset));
  bswapl(reg);
  shrl(reg, 16);
}


void InterpreterMacroAssembler::get_cache_index_at_bcp(Register index,
                                                       int bcp_offset,
                                                       size_t index_size) {
  assert(bcp_offset > 0, "bcp is still pointing to start of bytecode");
  if (index_size == sizeof(u2)) {
    load_unsigned_short(index, Address(r13, bcp_offset));
  } else if (index_size == sizeof(u4)) {
    assert(EnableInvokeDynamic, "giant index used only for JSR 292");
    movl(index, Address(r13, bcp_offset));
    // Check if the secondary index definition is still ~x, otherwise
    // we have to change the following assembler code to calculate the
    // plain index.
    assert(ConstantPool::decode_invokedynamic_index(~123) == 123, "else change next line");
    notl(index);  // convert to plain index
  } else if (index_size == sizeof(u1)) {
    load_unsigned_byte(index, Address(r13, bcp_offset));
  } else {
    ShouldNotReachHere();
  }
}


void InterpreterMacroAssembler::get_cache_and_index_at_bcp(Register cache,
                                                           Register index,
                                                           int bcp_offset,
                                                           size_t index_size) {
  assert_different_registers(cache, index);
  get_cache_index_at_bcp(index, bcp_offset, index_size);
  movptr(cache, Address(rbp, frame::interpreter_frame_cache_offset * wordSize));
  assert(sizeof(ConstantPoolCacheEntry) == 4 * wordSize, "adjust code below");
  // convert from field index to ConstantPoolCacheEntry index
  assert(exact_log2(in_words(ConstantPoolCacheEntry::size())) == 2, "else change next line");
  shll(index, 2);
}


void InterpreterMacroAssembler::get_cache_and_index_and_bytecode_at_bcp(Register cache,
                                                                        Register index,
                                                                        Register bytecode,
                                                                        int byte_no,
                                                                        int bcp_offset,
                                                                        size_t index_size) {
  get_cache_and_index_at_bcp(cache, index, bcp_offset, index_size);
  // We use a 32-bit load here since the layout of 64-bit words on
  // little-endian machines allow us that.
  movl(bytecode, Address(cache, index, Address::times_ptr, ConstantPoolCache::base_offset() + ConstantPoolCacheEntry::indices_offset()));
  const int shift_count = (1 + byte_no) * BitsPerByte;
  assert((byte_no == TemplateTable::f1_byte && shift_count == ConstantPoolCacheEntry::bytecode_1_shift) ||
         (byte_no == TemplateTable::f2_byte && shift_count == ConstantPoolCacheEntry::bytecode_2_shift),
         "correct shift count");
  shrl(bytecode, shift_count);
  assert(ConstantPoolCacheEntry::bytecode_1_mask == ConstantPoolCacheEntry::bytecode_2_mask, "common mask");
  andl(bytecode, ConstantPoolCacheEntry::bytecode_1_mask);
}


void InterpreterMacroAssembler::get_cache_entry_pointer_at_bcp(Register cache,
                                                               Register tmp,
                                                               int bcp_offset,
                                                               size_t index_size) {
  assert(cache != tmp, "must use different register");
  get_cache_index_at_bcp(tmp, bcp_offset, index_size);
  assert(sizeof(ConstantPoolCacheEntry) == 4 * wordSize, "adjust code below");
  // convert from field index to ConstantPoolCacheEntry index
  // and from word offset to byte offset
  assert(exact_log2(in_bytes(ConstantPoolCacheEntry::size_in_bytes())) == 2 + LogBytesPerWord, "else change next line");
  shll(tmp, 2 + LogBytesPerWord);
  movptr(cache, Address(rbp, frame::interpreter_frame_cache_offset * wordSize));
  // skip past the header
  addptr(cache, in_bytes(ConstantPoolCache::base_offset()));
  addptr(cache, tmp);  // construct pointer to cache entry
}

void InterpreterMacroAssembler::get_method_counters(Register method,
                                                    Register mcs, Label& skip) {
  Label has_counters;
  movptr(mcs, Address(method, Method::method_counters_offset()));
  testptr(mcs, mcs);
  jcc(Assembler::notZero, has_counters);
  call_VM(noreg, CAST_FROM_FN_PTR(address,
          InterpreterRuntime::build_method_counters), method);
  movptr(mcs, Address(method,Method::method_counters_offset()));
  testptr(mcs, mcs);
  jcc(Assembler::zero, skip); // No MethodCounters allocated, OutOfMemory
  bind(has_counters);
}

// Load object from cpool->resolved_references(index)
void InterpreterMacroAssembler::load_resolved_reference_at_index(
                                           Register result, Register index) {
  assert_different_registers(result, index);
  // convert from field index to resolved_references() index and from
  // word index to byte offset. Since this is a java object, it can be compressed
  Register tmp = index;  // reuse
  shll(tmp, LogBytesPerHeapOop);

  get_constant_pool(result);
  // load pointer for resolved_references[] objArray
  movptr(result, Address(result, ConstantPool::resolved_references_offset_in_bytes()));
  // JNIHandles::resolve(obj);
  movptr(result, Address(result, 0));
  // Add in the index
  addptr(result, tmp);
  load_heap_oop(result, Address(result, arrayOopDesc::base_offset_in_bytes(T_OBJECT)));
}

// Generate a subtype check: branch to ok_is_subtype if sub_klass is a
// subtype of super_klass.
//
// Args:
//      rax: superklass
//      Rsub_klass: subklass
//
// Kills:
//      rcx, rdi
void InterpreterMacroAssembler::gen_subtype_check(Register Rsub_klass,
                                                  Label& ok_is_subtype) {
  assert(Rsub_klass != rax, "rax holds superklass");
  assert(Rsub_klass != r14, "r14 holds locals");
  assert(Rsub_klass != r13, "r13 holds bcp");
  assert(Rsub_klass != rcx, "rcx holds 2ndary super array length");
  assert(Rsub_klass != rdi, "rdi holds 2ndary super array scan ptr");

  // Profile the not-null value's klass.
  profile_typecheck(rcx, Rsub_klass, rdi); // blows rcx, reloads rdi

  // Do the check.
  check_klass_subtype(Rsub_klass, rax, rcx, ok_is_subtype); // blows rcx

  // Profile the failure of the check.
  profile_typecheck_failed(rcx); // blows rcx
}



// Java Expression Stack

void InterpreterMacroAssembler::pop_ptr(Register r) {
  pop(r);
}

void InterpreterMacroAssembler::pop_i(Register r) {
  // XXX can't use pop currently, upper half non clean
  movl(r, Address(rsp, 0));
  addptr(rsp, wordSize);
}

void InterpreterMacroAssembler::pop_l(Register r) {
  movq(r, Address(rsp, 0));
  addptr(rsp, 2 * Interpreter::stackElementSize);
}

void InterpreterMacroAssembler::pop_f(XMMRegister r) {
  movflt(r, Address(rsp, 0));
  addptr(rsp, wordSize);
}

void InterpreterMacroAssembler::pop_d(XMMRegister r) {
  movdbl(r, Address(rsp, 0));
  addptr(rsp, 2 * Interpreter::stackElementSize);
}

void InterpreterMacroAssembler::push_ptr(Register r) {
  push(r);
}

void InterpreterMacroAssembler::push_i(Register r) {
  push(r);
}

void InterpreterMacroAssembler::push_l(Register r) {
  subptr(rsp, 2 * wordSize);
  movq(Address(rsp, 0), r);
}

void InterpreterMacroAssembler::push_f(XMMRegister r) {
  subptr(rsp, wordSize);
  movflt(Address(rsp, 0), r);
}

void InterpreterMacroAssembler::push_d(XMMRegister r) {
  subptr(rsp, 2 * wordSize);
  movdbl(Address(rsp, 0), r);
}

void InterpreterMacroAssembler::pop(TosState state) {
  switch (state) {
  case atos: pop_ptr();                 break;
  case btos:
  case ztos:
  case ctos:
  case stos:
  case itos: pop_i();                   break;
  case ltos: pop_l();                   break;
  case ftos: pop_f();                   break;
  case dtos: pop_d();                   break;
  case vtos: /* nothing to do */        break;
  default:   ShouldNotReachHere();
  }
  verify_oop(rax, state);
}

void InterpreterMacroAssembler::push(TosState state) {
  verify_oop(rax, state);
  switch (state) {
  case atos: push_ptr();                break;
  case btos:
  case ztos:
  case ctos:
  case stos:
  case itos: push_i();                  break;
  case ltos: push_l();                  break;
  case ftos: push_f();                  break;
  case dtos: push_d();                  break;
  case vtos: /* nothing to do */        break;
  default  : ShouldNotReachHere();
  }
}


// Helpers for swap and dup
void InterpreterMacroAssembler::load_ptr(int n, Register val) {
  movptr(val, Address(rsp, Interpreter::expr_offset_in_bytes(n)));
}

void InterpreterMacroAssembler::store_ptr(int n, Register val) {
  movptr(Address(rsp, Interpreter::expr_offset_in_bytes(n)), val);
}


void InterpreterMacroAssembler::prepare_to_jump_from_interpreted() {
  // set sender sp
  lea(r13, Address(rsp, wordSize));
  // record last_sp
  movptr(Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize), r13);
}


// Jump to from_interpreted entry of a call unless single stepping is possible
// in this thread in which case we must call the i2i entry
void InterpreterMacroAssembler::jump_from_interpreted(Register method, Register temp) {
  prepare_to_jump_from_interpreted();
  movl(r12, Address(method, Method::kind_offset()));
  movptr(rax, (intptr_t)AbstractInterpreter::_entry_table);
  movptr(r12, Address(rax, r12, Address::times_8));
  movl(r11, 0);
  jmp(r12);
}


// The following two routines provide a hook so that an implementation
// can schedule the dispatch in two parts.  amd64 does not do this.
void InterpreterMacroAssembler::dispatch_prolog(TosState state, int step) {
  // Nothing amd64 specific to be done here
}

void InterpreterMacroAssembler::dispatch_epilog(TosState state, int step) {
  dispatch_next(state, step);
}

void InterpreterMacroAssembler::dispatch_base(TosState state,
                                              address* table,
                                              bool verifyoop) {
  verify_FPU(1, state);
  if (VerifyActivationFrameSize) {
    Label L;
    mov(rcx, rbp);
    subptr(rcx, rsp);
    int32_t min_frame_size =
      (frame::link_offset - frame::interpreter_frame_initial_sp_offset) *
      wordSize;
    cmpptr(rcx, (int32_t)min_frame_size);
    jcc(Assembler::greaterEqual, L);
    stop("broken stack frame");
    bind(L);
  }
  if (verifyoop) {
    verify_oop(rax, state);
  }
  lea(rscratch1, ExternalAddress((address)table));
  jmp(Address(rscratch1, rbx, Address::times_8));
}

void InterpreterMacroAssembler::dispatch_only(TosState state) {
  dispatch_base(state, Interpreter::dispatch_table(state));
}

void InterpreterMacroAssembler::dispatch_only_normal(TosState state) {
  dispatch_base(state, Interpreter::normal_table(state));
}

void InterpreterMacroAssembler::dispatch_only_noverify(TosState state) {
  dispatch_base(state, Interpreter::normal_table(state), false);
}


void InterpreterMacroAssembler::dispatch_next(TosState state, int step) {
  // load next bytecode (load before advancing r13 to prevent AGI)
  load_unsigned_byte(rbx, Address(r13, step));
  // advance r13
  increment(r13, step);
  dispatch_base(state, Interpreter::dispatch_table(state));
}

void InterpreterMacroAssembler::dispatch_via(TosState state, address* table) {
  // load current bytecode
  load_unsigned_byte(rbx, Address(r13, 0));
  dispatch_base(state, table);
}

// remove activation
//
// Unlock the receiver if this is a synchronized method.
// Unlock any Java monitors from syncronized blocks.
// Remove the activation from the stack.
//
// If there are locked Java monitors
//    If throw_monitor_exception
//       throws IllegalMonitorStateException
//    Else if install_monitor_exception
//       installs IllegalMonitorStateException
//    Else
//       no error processing
void InterpreterMacroAssembler::remove_activation(
        TosState state,
        Register ret_addr,
        bool throw_monitor_exception,
        bool install_monitor_exception,
        bool notify_jvmdi) {
  // Note: Registers rdx xmm0 may be in use for the
  // result check if synchronized method
  Label unlocked, unlock, no_unlock;

  // get the value of _do_not_unlock_if_synchronized into rdx
  const Address do_not_unlock_if_synchronized(r15_thread,
    in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
  movbool(rdx, do_not_unlock_if_synchronized);
  movbool(do_not_unlock_if_synchronized, false); // reset the flag

 // get method access flags
  movptr(rbx, Address(rbp, frame::interpreter_frame_method_offset * wordSize));
  movl(rcx, Address(rbx, Method::access_flags_offset()));
  testl(rcx, JVM_ACC_SYNCHRONIZED);
  jcc(Assembler::zero, unlocked);

  // Don't unlock anything if the _do_not_unlock_if_synchronized flag
  // is set.
  testbool(rdx);
  jcc(Assembler::notZero, no_unlock);

  // unlock monitor
  push(state); // save result

  // BasicObjectLock will be first in list, since this is a
  // synchronized method. However, need to check that the object has
  // not been unlocked by an explicit monitorexit bytecode.
  const Address monitor(rbp, frame::interpreter_frame_initial_sp_offset *
                        wordSize - (int) sizeof(BasicObjectLock));
  // We use c_rarg1 so that if we go slow path it will be the correct
  // register for unlock_object to pass to VM directly
  lea(c_rarg1, monitor); // address of first monitor

  movptr(rax, Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()));
  testptr(rax, rax);
  jcc(Assembler::notZero, unlock);

  pop(state);
  if (throw_monitor_exception) {
    // Entry already unlocked, need to throw exception
    call_VM(noreg, CAST_FROM_FN_PTR(address,
                   InterpreterRuntime::throw_illegal_monitor_state_exception));
    should_not_reach_here();
  } else {
    // Monitor already unlocked during a stack unroll. If requested,
    // install an illegal_monitor_state_exception.  Continue with
    // stack unrolling.
    if (install_monitor_exception) {
      call_VM(noreg, CAST_FROM_FN_PTR(address,
                     InterpreterRuntime::new_illegal_monitor_state_exception));
    }
    jmp(unlocked);
  }

  bind(unlock);
  unlock_object(c_rarg1);
  pop(state);

  // Check that for block-structured locking (i.e., that all locked
  // objects has been unlocked)
  bind(unlocked);

  // rax: Might contain return value

  // Check that all monitors are unlocked
  {
    Label loop, exception, entry, restart;
    const int entry_size = frame::interpreter_frame_monitor_size() * wordSize;
    const Address monitor_block_top(
        rbp, frame::interpreter_frame_monitor_block_top_offset * wordSize);
    const Address monitor_block_bot(
        rbp, frame::interpreter_frame_initial_sp_offset * wordSize);

    bind(restart);
    // We use c_rarg1 so that if we go slow path it will be the correct
    // register for unlock_object to pass to VM directly
    movptr(c_rarg1, monitor_block_top); // points to current entry, starting
                                  // with top-most entry
    lea(rbx, monitor_block_bot);  // points to word before bottom of
                                  // monitor block
    jmp(entry);

    // Entry already locked, need to throw exception
    bind(exception);

    if (throw_monitor_exception) {
      // Throw exception
      MacroAssembler::call_VM(noreg,
                              CAST_FROM_FN_PTR(address, InterpreterRuntime::
                                   throw_illegal_monitor_state_exception));
      should_not_reach_here();
    } else {
      // Stack unrolling. Unlock object and install illegal_monitor_exception.
      // Unlock does not block, so don't have to worry about the frame.
      // We don't have to preserve c_rarg1 since we are going to throw an exception.

      push(state);
      unlock_object(c_rarg1);
      pop(state);

      if (install_monitor_exception) {
        call_VM(noreg, CAST_FROM_FN_PTR(address,
                                        InterpreterRuntime::
                                        new_illegal_monitor_state_exception));
      }

      jmp(restart);
    }

    bind(loop);
    // check if current entry is used
    cmpptr(Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()), (int32_t) NULL);
    jcc(Assembler::notEqual, exception);

    addptr(c_rarg1, entry_size); // otherwise advance to next entry
    bind(entry);
    cmpptr(c_rarg1, rbx); // check if bottom reached
    jcc(Assembler::notEqual, loop); // if not at bottom then check this entry
  }

  bind(no_unlock);

  // jvmti support
  if (notify_jvmdi) {
    notify_method_exit(state, NotifyJVMTI);    // preserve TOSCA
  } else {
    notify_method_exit(state, SkipNotifyJVMTI); // preserve TOSCA
  }

  // remove activation
  // get sender sp
  movptr(rbx,
         Address(rbp, frame::interpreter_frame_sender_sp_offset * wordSize));
  leave();                           // remove frame anchor
  pop(ret_addr);                     // get return address
  mov(rsp, rbx);                     // set sp to sender sp
}

#endif // C_INTERP

// Lock object
//
// Args:
//      c_rarg1: BasicObjectLock to be used for locking
//
// Kills:
//      rax
//      c_rarg0, c_rarg1, c_rarg2, c_rarg3, .. (param regs)
//      rscratch1, rscratch2 (scratch regs)
void InterpreterMacroAssembler::lock_object(Register lock_reg) {
  assert(lock_reg == c_rarg1, "The argument is only for looks. It must be c_rarg1");

  if (UseHeavyMonitors) {
    call_VM(noreg,
            CAST_FROM_FN_PTR(address, InterpreterRuntime::monitorenter),
            lock_reg);
  } else {
    Label done;

    const Register swap_reg = rax; // Must use rax for cmpxchg instruction
    const Register obj_reg = c_rarg3; // Will contain the oop

    const int obj_offset = BasicObjectLock::obj_offset_in_bytes();
    const int lock_offset = BasicObjectLock::lock_offset_in_bytes ();
    const int mark_offset = lock_offset +
                            BasicLock::displaced_header_offset_in_bytes();

    Label slow_case;

    // Load object pointer into obj_reg %c_rarg3
    movptr(obj_reg, Address(lock_reg, obj_offset));

    if (UseBiasedLocking) {
      biased_locking_enter(lock_reg, obj_reg, swap_reg, rscratch1, false, done, &slow_case);
    }

    // Load immediate 1 into swap_reg %rax
    movl(swap_reg, 1);

    // Load (object->mark() | 1) into swap_reg %rax
    orptr(swap_reg, Address(obj_reg, 0));

    // Save (object->mark() | 1) into BasicLock's displaced header
    movptr(Address(lock_reg, mark_offset), swap_reg);

    assert(lock_offset == 0,
           "displached header must be first word in BasicObjectLock");

    if (os::is_MP()) lock();
    cmpxchgptr(lock_reg, Address(obj_reg, 0));
    jcc(Assembler::zero, done);

    // Test if the oopMark is an obvious stack pointer, i.e.,
    //  1) (mark & 7) == 0, and
    //  2) rsp <= mark < mark + os::pagesize()
    //
    // These 3 tests can be done by evaluating the following
    // expression: ((mark - rsp) & (7 - os::vm_page_size())),
    // assuming both stack pointer and pagesize have their
    // least significant 3 bits clear.
    // NOTE: the oopMark is in swap_reg %rax as the result of cmpxchg
    subptr(swap_reg, rsp);
    andptr(swap_reg, 7 - os::vm_page_size());

    // Save the test result, for recursive case, the result is zero
    movptr(Address(lock_reg, mark_offset), swap_reg);

    jcc(Assembler::zero, done);

    bind(slow_case);

    // Call the runtime routine for slow case
    call_VM(noreg,
            CAST_FROM_FN_PTR(address, InterpreterRuntime::monitorenter),
            lock_reg);

    bind(done);
  }
}


// Unlocks an object. Used in monitorexit bytecode and
// remove_activation.  Throws an IllegalMonitorException if object is
// not locked by current thread.
//
// Args:
//      c_rarg1: BasicObjectLock for lock
//
// Kills:
//      rax
//      c_rarg0, c_rarg1, c_rarg2, c_rarg3, ... (param regs)
//      rscratch1, rscratch2 (scratch regs)
void InterpreterMacroAssembler::unlock_object(Register lock_reg) {
  assert(lock_reg == c_rarg1, "The argument is only for looks. It must be rarg1");

  if (UseHeavyMonitors) {
    call_VM(noreg,
            CAST_FROM_FN_PTR(address, InterpreterRuntime::monitorexit),
            lock_reg);
  } else {
    Label done;

    const Register swap_reg   = rax;  // Must use rax for cmpxchg instruction
    const Register header_reg = c_rarg2;  // Will contain the old oopMark
    const Register obj_reg    = c_rarg3;  // Will contain the oop

    save_bcp(); // Save in case of exception

    // Convert from BasicObjectLock structure to object and BasicLock
    // structure Store the BasicLock address into %rax
    lea(swap_reg, Address(lock_reg, BasicObjectLock::lock_offset_in_bytes()));

    // Load oop into obj_reg(%c_rarg3)
    movptr(obj_reg, Address(lock_reg, BasicObjectLock::obj_offset_in_bytes()));

    // Free entry
    movptr(Address(lock_reg, BasicObjectLock::obj_offset_in_bytes()), (int32_t)NULL_WORD);

    if (UseBiasedLocking) {
      biased_locking_exit(obj_reg, header_reg, done);
    }

    // Load the old header from BasicLock structure
    movptr(header_reg, Address(swap_reg,
                               BasicLock::displaced_header_offset_in_bytes()));

    // Test for recursion
    testptr(header_reg, header_reg);

    // zero for recursive case
    jcc(Assembler::zero, done);

    // Atomic swap back the old header
    if (os::is_MP()) lock();
    cmpxchgptr(header_reg, Address(obj_reg, 0));

    // zero for recursive case
    jcc(Assembler::zero, done);

    // Call the runtime routine for slow case.
    movptr(Address(lock_reg, BasicObjectLock::obj_offset_in_bytes()),
         obj_reg); // restore obj
    call_VM(noreg,
            CAST_FROM_FN_PTR(address, InterpreterRuntime::monitorexit),
            lock_reg);

    bind(done);

    restore_bcp();
  }
}

#ifndef CC_INTERP

void InterpreterMacroAssembler::test_method_data_pointer(Register mdp,
                                                         Label& zero_continue) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  movptr(mdp, Address(rbp, frame::interpreter_frame_mdx_offset * wordSize));
  testptr(mdp, mdp);
  jcc(Assembler::zero, zero_continue);
}


// Set the method data pointer for the current bcp.
void InterpreterMacroAssembler::set_method_data_pointer_for_bcp() {
  assert(ProfileInterpreter, "must be profiling interpreter");
  Label set_mdp;
  push(rax);
  push(rbx);

  get_method(rbx);
  // Test MDO to avoid the call if it is NULL.
  movptr(rax, Address(rbx, in_bytes(Method::method_data_offset())));
  testptr(rax, rax);
  jcc(Assembler::zero, set_mdp);
  // rbx: method
  // r13: bcp
  call_VM_leaf(CAST_FROM_FN_PTR(address, InterpreterRuntime::bcp_to_di), rbx, r13);
  // rax: mdi
  // mdo is guaranteed to be non-zero here, we checked for it before the call.
  bind(set_mdp);
  movptr(Address(rbp, frame::interpreter_frame_mdx_offset * wordSize), rax);
  pop(rbx);
  pop(rax);
}

void InterpreterMacroAssembler::verify_method_data_pointer() {
  assert(ProfileInterpreter, "must be profiling interpreter");
#ifdef ASSERT
  Label verify_continue;
  push(rax);
  push(rbx);
  push(c_rarg3);
  push(c_rarg2);
  test_method_data_pointer(c_rarg3, verify_continue); // If mdp is zero, continue
  get_method(rbx);

  // If the mdp is valid, it will point to a DataLayout header which is
  // consistent with the bcp.  The converse is highly probable also.
  load_unsigned_short(c_rarg2,
                      Address(c_rarg3, in_bytes(DataLayout::bci_offset())));
  addptr(c_rarg2, Address(rbx, Method::const_offset()));
  lea(c_rarg2, Address(c_rarg2, ConstMethod::codes_offset()));
  cmpptr(c_rarg2, r13);
  jcc(Assembler::equal, verify_continue);
  // rbx: method
  // r13: bcp
  // c_rarg3: mdp
  call_VM_leaf(CAST_FROM_FN_PTR(address, InterpreterRuntime::verify_mdp),
               rbx, r13, c_rarg3);
  bind(verify_continue);
  pop(c_rarg2);
  pop(c_rarg3);
  pop(rbx);
  pop(rax);
#endif // ASSERT
}


void InterpreterMacroAssembler::set_mdp_data_at(Register mdp_in,
                                                int constant,
                                                Register value) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  Address data(mdp_in, constant);
  movptr(data, value);
}


void InterpreterMacroAssembler::increment_mdp_data_at(Register mdp_in,
                                                      int constant,
                                                      bool decrement) {
  // Counter address
  Address data(mdp_in, constant);

  increment_mdp_data_at(data, decrement);
}

void InterpreterMacroAssembler::increment_mdp_data_at(Address data,
                                                      bool decrement) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  // %%% this does 64bit counters at best it is wasting space
  // at worst it is a rare bug when counters overflow
  // TODO
}


void InterpreterMacroAssembler::increment_mdp_data_at(Register mdp_in,
                                                      Register reg,
                                                      int constant,
                                                      bool decrement) {
  Address data(mdp_in, reg, Address::times_1, constant);

  increment_mdp_data_at(data, decrement);
}

void InterpreterMacroAssembler::set_mdp_flag_at(Register mdp_in,
                                                int flag_byte_constant) {
  // TODO
}



void InterpreterMacroAssembler::test_mdp_data_at(Register mdp_in,
                                                 int offset,
                                                 Register value,
                                                 Register test_value_out,
                                                 Label& not_equal_continue) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  if (test_value_out == noreg) {
    cmpptr(value, Address(mdp_in, offset));
  } else {
    // Put the test value into a register, so caller can use it:
    movptr(test_value_out, Address(mdp_in, offset));
    cmpptr(test_value_out, value);
  }
  jcc(Assembler::notEqual, not_equal_continue);
}


void InterpreterMacroAssembler::update_mdp_by_offset(Register mdp_in,
                                                     int offset_of_disp) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  Address disp_address(mdp_in, offset_of_disp);
  addptr(mdp_in, disp_address);
  movptr(Address(rbp, frame::interpreter_frame_mdx_offset * wordSize), mdp_in);
}


void InterpreterMacroAssembler::update_mdp_by_offset(Register mdp_in,
                                                     Register reg,
                                                     int offset_of_disp) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  Address disp_address(mdp_in, reg, Address::times_1, offset_of_disp);
  addptr(mdp_in, disp_address);
  movptr(Address(rbp, frame::interpreter_frame_mdx_offset * wordSize), mdp_in);
}


void InterpreterMacroAssembler::update_mdp_by_constant(Register mdp_in,
                                                       int constant) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  addptr(mdp_in, constant);
  movptr(Address(rbp, frame::interpreter_frame_mdx_offset * wordSize), mdp_in);
}


void InterpreterMacroAssembler::update_mdp_for_ret(Register return_bci) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  push(return_bci); // save/restore across call_VM
  call_VM(noreg,
          CAST_FROM_FN_PTR(address, InterpreterRuntime::update_mdp_for_ret),
          return_bci);
  pop(return_bci);
}


void InterpreterMacroAssembler::profile_taken_branch(Register mdp,
                                                     Register bumped_count) {
//
}


void InterpreterMacroAssembler::profile_not_taken_branch(Register mdp) {
//
}

void InterpreterMacroAssembler::profile_call(Register mdp) {
//
}


void InterpreterMacroAssembler::profile_final_call(Register mdp) {
//
}


void InterpreterMacroAssembler::profile_virtual_call(Register receiver,
                                                     Register mdp,
                                                     Register reg2,
                                                     bool receiver_can_be_null) {
//
}

// This routine creates a state machine for updating the multi-row
// type profile at a virtual call site (or other type-sensitive bytecode).
// The machine visits each row (of receiver/count) until the receiver type
// is found, or until it runs out of rows.  At the same time, it remembers
// the location of the first empty row.  (An empty row records null for its
// receiver, and can be allocated for a newly-observed receiver type.)
// Because there are two degrees of freedom in the state, a simple linear
// search will not work; it must be a decision tree.  Hence this helper
// function is recursive, to generate the required tree structured code.
// It's the interpreter, so we are trading off code space for speed.
// See below for example code.
void InterpreterMacroAssembler::record_klass_in_profile_helper(
                                        Register receiver, Register mdp,
                                        Register reg2, int start_row,
                                        Label& done, bool is_virtual_call) {
//
}

// Example state machine code for three profile rows:
//   // main copy of decision tree, rooted at row[1]
//   if (row[0].rec == rec) { row[0].incr(); goto done; }
//   if (row[0].rec != NULL) {
//     // inner copy of decision tree, rooted at row[1]
//     if (row[1].rec == rec) { row[1].incr(); goto done; }
//     if (row[1].rec != NULL) {
//       // degenerate decision tree, rooted at row[2]
//       if (row[2].rec == rec) { row[2].incr(); goto done; }
//       if (row[2].rec != NULL) { count.incr(); goto done; } // overflow
//       row[2].init(rec); goto done;
//     } else {
//       // remember row[1] is empty
//       if (row[2].rec == rec) { row[2].incr(); goto done; }
//       row[1].init(rec); goto done;
//     }
//   } else {
//     // remember row[0] is empty
//     if (row[1].rec == rec) { row[1].incr(); goto done; }
//     if (row[2].rec == rec) { row[2].incr(); goto done; }
//     row[0].init(rec); goto done;
//   }
//   done:

void InterpreterMacroAssembler::record_klass_in_profile(Register receiver,
                                                        Register mdp, Register reg2,
                                                        bool is_virtual_call) {
  assert(ProfileInterpreter, "must be profiling");
  Label done;

  record_klass_in_profile_helper(receiver, mdp, reg2, 0, done, is_virtual_call);

  bind (done);
}

void InterpreterMacroAssembler::profile_ret(Register return_bci,
                                            Register mdp) {
//
}


void InterpreterMacroAssembler::profile_null_seen(Register mdp) {

}


void InterpreterMacroAssembler::profile_typecheck_failed(Register mdp) {

}


void InterpreterMacroAssembler::profile_typecheck(Register mdp, Register klass, Register reg2) {
//
}


void InterpreterMacroAssembler::profile_switch_default(Register mdp) {
//
}


void InterpreterMacroAssembler::profile_switch_case(Register index,
                                                    Register mdp,
                                                    Register reg2) {
//
}



void InterpreterMacroAssembler::verify_oop(Register reg, TosState state) {
  if (state == atos) {
    MacroAssembler::verify_oop(reg);
  }
}

void InterpreterMacroAssembler::verify_FPU(int stack_depth, TosState state) {
}
#endif // !CC_INTERP


void InterpreterMacroAssembler::notify_method_entry() {
  // Whenever JVMTI is interp_only_mode, method entry/exit events are sent to
  // track stack depth.  If it is possible to enter interp_only_mode we add
  // the code to check if the event should be sent.
  //
}


void InterpreterMacroAssembler::notify_method_exit(
    TosState state, NotifyMethodExitMode mode) {
//
}

// Jump if ((*counter_addr += increment) & mask) satisfies the condition.
void InterpreterMacroAssembler::increment_mask_and_jump(Address counter_addr,
                                                        int increment, int mask,
                                                        Register scratch, bool preloaded,
                                                        Condition cond, Label* where) {
  if (!preloaded) {
    movl(scratch, counter_addr);
  }
  incrementl(scratch, increment);
  movl(counter_addr, scratch);
  andl(scratch, mask);
  jcc(cond, *where);
}
