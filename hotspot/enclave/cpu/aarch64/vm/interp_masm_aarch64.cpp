/*
 * Copyright (c) 2013, Red Hat Inc.
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates.
 * All rights reserved.
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
#include "interp_masm_aarch64.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "oops/arrayOop.hpp"
#include "oops/markOop.hpp"
// #include "oops/methodData.hpp"
#include "oops/method.hpp"
// #include "prims/jvmtiExport.hpp"
// #include "prims/jvmtiRedefineClassesTrace.hpp"
// #include "prims/jvmtiThreadState.hpp"
// #include "runtime/basicLock.hpp"
// #include "runtime/biasedLocking.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/thread.inline.hpp"


void InterpreterMacroAssembler::narrow(Register result) {

  // Get method->_constMethod->_result_type
  ldr(rscratch1, Address(rfp, frame::interpreter_frame_method_offset * wordSize));
  ldr(rscratch1, Address(rscratch1, Method::const_offset()));
  ldrb(rscratch1, Address(rscratch1, ConstMethod::result_type_offset()));

  Label done, notBool, notByte, notChar;

  // common case first
  cmpw(rscratch1, T_INT);
  br(Assembler::EQ, done);

  // mask integer result to narrower return type.
  cmpw(rscratch1, T_BOOLEAN);
  br(Assembler::NE, notBool);
  andw(result, result, 0x1);
  b(done);

  bind(notBool);
  cmpw(rscratch1, T_BYTE);
  br(Assembler::NE, notByte);
  sbfx(result, result, 0, 8);
  b(done);

  bind(notByte);
  cmpw(rscratch1, T_CHAR);
  br(Assembler::NE, notChar);
  ubfx(result, result, 0, 16);  // truncate upper 16 bits
  b(done);

  bind(notChar);
  sbfx(result, result, 0, 16);     // sign-extend short

  // Nothing to do for T_INT
  bind(done);
}

#ifndef CC_INTERP

void InterpreterMacroAssembler::check_and_handle_popframe(Register java_thread) {

}


void InterpreterMacroAssembler::load_earlyret_value(TosState state) {
 
}


void InterpreterMacroAssembler::check_and_handle_earlyret(Register java_thread) {

}

void InterpreterMacroAssembler::get_unsigned_2_byte_index_at_bcp(
  Register reg,
  int bcp_offset) {
  assert(bcp_offset >= 0, "bcp is still pointing to start of bytecode");
  ldrh(reg, Address(rbcp, bcp_offset));
  rev16(reg, reg);
}

void InterpreterMacroAssembler::get_dispatch() {
  unsigned long offset;
  adrp(rdispatch, ExternalAddress((address)Interpreter::dispatch_table()), offset);
  lea(rdispatch, Address(rdispatch, offset));
}

void InterpreterMacroAssembler::get_cache_index_at_bcp(Register index,
                                                       int bcp_offset,
                                                       size_t index_size) {
  assert(bcp_offset > 0, "bcp is still pointing to start of bytecode");
  if (index_size == sizeof(u2)) {
    load_unsigned_short(index, Address(rbcp, bcp_offset));
  } else if (index_size == sizeof(u4)) {
    assert(EnableInvokeDynamic, "giant index used only for JSR 292");
    ldrw(index, Address(rbcp, bcp_offset));
    // Check if the secondary index definition is still ~x, otherwise
    // we have to change the following assembler code to calculate the
    // plain index.
    assert(ConstantPool::decode_invokedynamic_index(~123) == 123, "else change next line");
    eonw(index, index, zr);  // convert to plain index
  } else if (index_size == sizeof(u1)) {
    load_unsigned_byte(index, Address(rbcp, bcp_offset));
  } else {
    ShouldNotReachHere();
  }
}

// Return
// Rindex: index into constant pool
// Rcache: address of cache entry - ConstantPoolCache::base_offset()
//
// A caller must add ConstantPoolCache::base_offset() to Rcache to get
// the true address of the cache entry.
//
void InterpreterMacroAssembler::get_cache_and_index_at_bcp(Register cache,
                                                           Register index,
                                                           int bcp_offset,
                                                           size_t index_size) {
  assert_different_registers(cache, index);
  assert_different_registers(cache, rcpool);
  get_cache_index_at_bcp(index, bcp_offset, index_size);
  assert(sizeof(ConstantPoolCacheEntry) == 4 * wordSize, "adjust code below");
  // convert from field index to ConstantPoolCacheEntry
  // aarch64 already has the cache in rcpool so there is no need to
  // install it in cache. instead we pre-add the indexed offset to
  // rcpool and return it in cache. All clients of this method need to
  // be modified accordingly.
  add(cache, rcpool, index, Assembler::LSL, 5);
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
  // n.b. unlike x86 cache already includes the index offset
  lea(bytecode, Address(cache,
			 ConstantPoolCache::base_offset()
			 + ConstantPoolCacheEntry::indices_offset()));
  ldarw(bytecode, bytecode);
  const int shift_count = (1 + byte_no) * BitsPerByte;
  ubfx(bytecode, bytecode, shift_count, BitsPerByte);
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
  ldr(cache, Address(rfp, frame::interpreter_frame_cache_offset * wordSize));
  // skip past the header
  add(cache, cache, in_bytes(ConstantPoolCache::base_offset()));
  add(cache, cache, tmp, Assembler::LSL, 2 + LogBytesPerWord);  // construct pointer to cache entry
}

void InterpreterMacroAssembler::get_method_counters(Register method,
                                                    Register mcs, Label& skip) {
  Label has_counters;
  ldr(mcs, Address(method, Method::method_counters_offset()));
  cbnz(mcs, has_counters);
  call_VM(noreg, CAST_FROM_FN_PTR(address,
          InterpreterRuntime::build_method_counters), method);
  ldr(mcs, Address(method, Method::method_counters_offset()));
  cbz(mcs, skip); // No MethodCounters allocated, OutOfMemory
  bind(has_counters);
}

// Load object from cpool->resolved_references(index)
void InterpreterMacroAssembler::load_resolved_reference_at_index(
                                           Register result, Register index) {
  assert_different_registers(result, index);
  // convert from field index to resolved_references() index and from
  // word index to byte offset. Since this is a java object, it can be compressed
  Register tmp = index;  // reuse
  lslw(tmp, tmp, LogBytesPerHeapOop);

  get_constant_pool(result);
  // load pointer for resolved_references[] objArray
  ldr(result, Address(result, ConstantPool::resolved_references_offset_in_bytes()));
  // JNIHandles::resolve(obj);
  ldr(result, Address(result, 0));
  // oopDesc::bs()->interpreter_read_barrier_not_null(this, result);
  // Add in the index
  add(result, result, tmp);
  load_heap_oop(result, Address(result, arrayOopDesc::base_offset_in_bytes(T_OBJECT)));
}

// Generate a subtype check: branch to ok_is_subtype if sub_klass is a
// subtype of super_klass.
//
// Args:
//      r0: superklass
//      Rsub_klass: subklass
//
// Kills:
//      r2, r5
void InterpreterMacroAssembler::gen_subtype_check(Register Rsub_klass,
                                                  Label& ok_is_subtype) {
  assert(Rsub_klass != r0, "r0 holds superklass");
  assert(Rsub_klass != r2, "r2 holds 2ndary super array length");
  assert(Rsub_klass != r5, "r5 holds 2ndary super array scan ptr");

  // Profile the not-null value's klass.
  profile_typecheck(r2, Rsub_klass, r5); // blows r2, reloads r5

  // Do the check.
  check_klass_subtype(Rsub_klass, r0, r2, ok_is_subtype); // blows r2

  // Profile the failure of the check.
  profile_typecheck_failed(r2); // blows r2
}

// Java Expression Stack

void InterpreterMacroAssembler::pop_ptr(Register r) {
  ldr(r, post(esp, wordSize));
}

void InterpreterMacroAssembler::pop_i(Register r) {
  ldrw(r, post(esp, wordSize));
}

void InterpreterMacroAssembler::pop_l(Register r) {
  ldr(r, post(esp, 2 * Interpreter::stackElementSize));
}

void InterpreterMacroAssembler::push_ptr(Register r) {
  str(r, pre(esp, -wordSize));
 }

void InterpreterMacroAssembler::push_i(Register r) {
  str(r, pre(esp, -wordSize));
}

void InterpreterMacroAssembler::push_l(Register r) {
  str(r, pre(esp, 2 * -wordSize));
}

void InterpreterMacroAssembler::pop_f(FloatRegister r) {
  ldrs(r, post(esp, wordSize));
}

void InterpreterMacroAssembler::pop_d(FloatRegister r) {
  ldrd(r, post(esp, 2 * Interpreter::stackElementSize));
}

void InterpreterMacroAssembler::push_f(FloatRegister r) {
  strs(r, pre(esp, -wordSize));
}

void InterpreterMacroAssembler::push_d(FloatRegister r) {
  strd(r, pre(esp, 2* -wordSize));
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
  verify_oop(r0, state);
}

void InterpreterMacroAssembler::push(TosState state) {
  verify_oop(r0, state);
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
  ldr(val, Address(esp, Interpreter::expr_offset_in_bytes(n)));
}

void InterpreterMacroAssembler::store_ptr(int n, Register val) {
  str(val, Address(esp, Interpreter::expr_offset_in_bytes(n)));
}


void InterpreterMacroAssembler::prepare_to_jump_from_interpreted() {
  // set sender sp
  mov(r13, sp);
  // record last_sp
  str(esp, Address(rfp, frame::interpreter_frame_last_sp_offset * wordSize));
}

// Jump to from_interpreted entry of a call unless single stepping is possible
// in this thread in which case we must call the i2i entry
void InterpreterMacroAssembler::jump_from_interpreted(Register method, Register temp) {
  prepare_to_jump_from_interpreted();

  ldr(rscratch1, Address(method, Method::from_interpreted_offset()));
  br(rscratch1);
}

// The following two routines provide a hook so that an implementation
// can schedule the dispatch in two parts.  amd64 does not do this.
void InterpreterMacroAssembler::dispatch_prolog(TosState state, int step) {
}

void InterpreterMacroAssembler::dispatch_epilog(TosState state, int step) {
    dispatch_next(state, step);
}

void InterpreterMacroAssembler::dispatch_base(TosState state,
                                              address* table,
                                              bool verifyoop) {
  if (VerifyActivationFrameSize) {
    Unimplemented();
  }
  if (verifyoop) {
    verify_oop(r0, state);
  }
  if (table == Interpreter::dispatch_table(state)) {
    addw(rscratch2, rscratch1, Interpreter::distance_from_dispatch_table(state));
    ldr(rscratch2, Address(rdispatch, rscratch2, Address::uxtw(3)));
  } else {
    mov(rscratch2, (address)table);
    ldr(rscratch2, Address(rscratch2, rscratch1, Address::uxtw(3)));
  }
  br(rscratch2);
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
  // load next bytecode
  ldrb(rscratch1, Address(pre(rbcp, step)));
  dispatch_base(state, Interpreter::dispatch_table(state));
}

void InterpreterMacroAssembler::dispatch_via(TosState state, address* table) {
  // load current bytecode
  ldrb(rscratch1, Address(rbcp, 0));
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
        bool throw_monitor_exception,
        bool install_monitor_exception,
        bool notify_jvmdi) {
  // Note: Registers r3 xmm0 may be in use for the
  // result check if synchronized method
  Label unlocked, unlock, no_unlock;

  // get the value of _do_not_unlock_if_synchronized into r3
  const Address do_not_unlock_if_synchronized(rthread,
    in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
  ldrb(r3, do_not_unlock_if_synchronized);
  strb(zr, do_not_unlock_if_synchronized); // reset the flag

 // get method access flags
  ldr(r1, Address(rfp, frame::interpreter_frame_method_offset * wordSize));
  ldr(r2, Address(r1, Method::access_flags_offset()));
  tst(r2, JVM_ACC_SYNCHRONIZED);
  br(Assembler::EQ, unlocked);

  // Don't unlock anything if the _do_not_unlock_if_synchronized flag
  // is set.
  cbnz(r3, no_unlock);

  // unlock monitor
  push(state); // save result

  // BasicObjectLock will be first in list, since this is a
  // synchronized method. However, need to check that the object has
  // not been unlocked by an explicit monitorexit bytecode.
  const Address monitor(rfp, frame::interpreter_frame_initial_sp_offset *
                        wordSize - (int) sizeof(BasicObjectLock));
  // We use c_rarg1 so that if we go slow path it will be the correct
  // register for unlock_object to pass to VM directly
  lea(c_rarg1, monitor); // address of first monitor

  ldr(r0, Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()));
  cbnz(r0, unlock);

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
    b(unlocked);
  }

  bind(unlock);
  unlock_object(c_rarg1);
  pop(state);

  // Check that for block-structured locking (i.e., that all locked
  // objects has been unlocked)
  bind(unlocked);

  // r0: Might contain return value

  // Check that all monitors are unlocked
  {
    Label loop, exception, entry, restart;
    const int entry_size = frame::interpreter_frame_monitor_size() * wordSize;
    const Address monitor_block_top(
        rfp, frame::interpreter_frame_monitor_block_top_offset * wordSize);
    const Address monitor_block_bot(
        rfp, frame::interpreter_frame_initial_sp_offset * wordSize);

    bind(restart);
    // We use c_rarg1 so that if we go slow path it will be the correct
    // register for unlock_object to pass to VM directly
    ldr(c_rarg1, monitor_block_top); // points to current entry, starting
                                     // with top-most entry
    lea(r19, monitor_block_bot);  // points to word before bottom of
                                  // monitor block
    b(entry);

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

      b(restart);
    }

    bind(loop);
    // check if current entry is used
    ldr(rscratch1, Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()));
    cbnz(rscratch1, exception);

    add(c_rarg1, c_rarg1, entry_size); // otherwise advance to next entry
    bind(entry);
    cmp(c_rarg1, r19); // check if bottom reached
    br(Assembler::NE, loop); // if not at bottom then check this entry
  }

  bind(no_unlock);

  // jvmti support
  if (notify_jvmdi) {
    notify_method_exit(state, NotifyJVMTI);    // preserve TOSCA
  } else {
    notify_method_exit(state, SkipNotifyJVMTI); // preserve TOSCA
  }

  // remove activation
  // get sender esp
  ldr(esp,
      Address(rfp, frame::interpreter_frame_sender_sp_offset * wordSize));
  // remove frame anchor
  leave();
  // If we're returning to interpreted code we will shortly be
  // adjusting SP to allow some space for ESP.  If we're returning to
  // compiled code the saved sender SP was saved in sender_sp, so this
  // restores it.
  andr(sp, esp, -16);
}

#endif // C_INTERP

// Lock object
//
// Args:
//      c_rarg1: BasicObjectLock to be used for locking
//
// Kills:
//      r0
//      c_rarg0, c_rarg1, c_rarg2, c_rarg3, .. (param regs)
//      rscratch1, rscratch2 (scratch regs)
void InterpreterMacroAssembler::lock_object(Register lock_reg)
{
  assert(lock_reg == c_rarg1, "The argument is only for looks. It must be c_rarg1");
  if (UseHeavyMonitors) {
    call_VM(noreg,
            CAST_FROM_FN_PTR(address, InterpreterRuntime::monitorenter),
            lock_reg);
  } else {
    Label done;

    const Register swap_reg = r0;
    const Register tmp = c_rarg2;
    const Register obj_reg = c_rarg3; // Will contain the oop

    const int obj_offset = BasicObjectLock::obj_offset_in_bytes();
    const int lock_offset = BasicObjectLock::lock_offset_in_bytes ();
    const int mark_offset = lock_offset +
                            BasicLock::displaced_header_offset_in_bytes();

    Label slow_case;

    // Load object pointer into obj_reg %c_rarg3
    ldr(obj_reg, Address(lock_reg, obj_offset));

    if (UseBiasedLocking) {
      biased_locking_enter(lock_reg, obj_reg, swap_reg, tmp, false, done, &slow_case);
    }

    // Load (object->mark() | 1) into swap_reg
    ldr(rscratch1, Address(obj_reg, 0));
    orr(swap_reg, rscratch1, 1);

    // Save (object->mark() | 1) into BasicLock's displaced header
    str(swap_reg, Address(lock_reg, mark_offset));

    assert(lock_offset == 0,
           "displached header must be first word in BasicObjectLock");

    Label fail;
    if (PrintBiasedLockingStatistics) {
      //
    } else {
      cmpxchgptr(swap_reg, lock_reg, obj_reg, rscratch1, done, /*fallthrough*/NULL);
    }

    // Test if the oopMark is an obvious stack pointer, i.e.,
    //  1) (mark & 7) == 0, and
    //  2) rsp <= mark < mark + os::pagesize()
    //
    // These 3 tests can be done by evaluating the following
    // expression: ((mark - rsp) & (7 - os::vm_page_size())),
    // assuming both stack pointer and pagesize have their
    // least significant 3 bits clear.
    // NOTE: the oopMark is in swap_reg %r0 as the result of cmpxchg
    // NOTE2: aarch64 does not like to subtract sp from rn so take a
    // copy
    mov(rscratch1, sp);
    sub(swap_reg, swap_reg, rscratch1);
    ands(swap_reg, swap_reg, (unsigned long)(7 - os::vm_page_size()));

    // Save the test result, for recursive case, the result is zero
    str(swap_reg, Address(lock_reg, mark_offset));

    if (PrintBiasedLockingStatistics) {
 
    }
    br(Assembler::EQ, done);

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
//      r0
//      c_rarg0, c_rarg1, c_rarg2, c_rarg3, ... (param regs)
//      rscratch1, rscratch2 (scratch regs)
void InterpreterMacroAssembler::unlock_object(Register lock_reg)
{
  assert(lock_reg == c_rarg1, "The argument is only for looks. It must be rarg1");

  if (UseHeavyMonitors) {
    call_VM(noreg,
            CAST_FROM_FN_PTR(address, InterpreterRuntime::monitorexit),
            lock_reg);
  } else {
    Label done;

    const Register swap_reg   = r0;
    const Register header_reg = c_rarg2;  // Will contain the old oopMark
    const Register obj_reg    = c_rarg3;  // Will contain the oop

    save_bcp(); // Save in case of exception

    // Convert from BasicObjectLock structure to object and BasicLock
    // structure Store the BasicLock address into %r0
    lea(swap_reg, Address(lock_reg, BasicObjectLock::lock_offset_in_bytes()));

    // Load oop into obj_reg(%c_rarg3)
    ldr(obj_reg, Address(lock_reg, BasicObjectLock::obj_offset_in_bytes()));

    // Free entry
    str(zr, Address(lock_reg, BasicObjectLock::obj_offset_in_bytes()));

    if (UseBiasedLocking) {
      biased_locking_exit(obj_reg, header_reg, done);
    }

    // Load the old header from BasicLock structure
    ldr(header_reg, Address(swap_reg,
			    BasicLock::displaced_header_offset_in_bytes()));

    // Test for recursion
    cbz(header_reg, done);

    // Atomic swap back the old header
    cmpxchgptr(swap_reg, header_reg, obj_reg, rscratch1, done, /*fallthrough*/NULL);

    // Call the runtime routine for slow case.
    str(obj_reg, Address(lock_reg, BasicObjectLock::obj_offset_in_bytes())); // restore obj
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
  ldr(mdp, Address(rfp, frame::interpreter_frame_mdx_offset * wordSize));
  cbz(mdp, zero_continue);
}

// Set the method data pointer for the current bcp.
void InterpreterMacroAssembler::set_method_data_pointer_for_bcp() {
  assert(ProfileInterpreter, "must be profiling interpreter");
  Label set_mdp;
  stp(r0, r1, Address(pre(sp, -2 * wordSize)));

  // Test MDO to avoid the call if it is NULL.
  ldr(r0, Address(rmethod, in_bytes(Method::method_data_offset())));
  cbz(r0, set_mdp);
  call_VM_leaf(CAST_FROM_FN_PTR(address, InterpreterRuntime::bcp_to_di), rmethod, rbcp);
  // r0: mdi
  // mdo is guaranteed to be non-zero here, we checked for it before the call.
  ldr(r1, Address(rmethod, in_bytes(Method::method_data_offset())));
  //TODO: fixme
  // lea(r1, Address(r1, in_bytes(MethodData::data_offset())));
  add(r0, r1, r0);
  str(r0, Address(rfp, frame::interpreter_frame_mdx_offset * wordSize));
  bind(set_mdp);
  ldp(r0, r1, Address(post(sp, 2 * wordSize)));
}

void InterpreterMacroAssembler::verify_method_data_pointer() {
  assert(ProfileInterpreter, "must be profiling interpreter");
#ifdef ASSERT
  Label verify_continue;
  stp(r0, r1, Address(pre(sp, -2 * wordSize)));
  stp(r2, r3, Address(pre(sp, -2 * wordSize)));
  test_method_data_pointer(r3, verify_continue); // If mdp is zero, continue
  get_method(r1);

  // If the mdp is valid, it will point to a DataLayout header which is
  // consistent with the bcp.  The converse is highly probable also.
  ldrsh(r2, Address(r3, in_bytes(DataLayout::bci_offset())));
  ldr(rscratch1, Address(r1, Method::const_offset()));
  add(r2, r2, rscratch1, Assembler::LSL);
  lea(r2, Address(r2, ConstMethod::codes_offset()));
  cmp(r2, rbcp);
  br(Assembler::EQ, verify_continue);
  // r1: method
  // rbcp: bcp // rbcp == 22
  // r3: mdp
  call_VM_leaf(CAST_FROM_FN_PTR(address, InterpreterRuntime::verify_mdp),
               r1, rbcp, r3);
  bind(verify_continue);
  ldp(r2, r3, Address(post(sp, 2 * wordSize)));
  ldp(r0, r1, Address(post(sp, 2 * wordSize)));
#endif // ASSERT
}


void InterpreterMacroAssembler::set_mdp_data_at(Register mdp_in,
                                                int constant,
                                                Register value) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  Address data(mdp_in, constant);
  str(value, data);
}


void InterpreterMacroAssembler::increment_mdp_data_at(Register mdp_in,
                                                      int constant,
                                                      bool decrement) {
  increment_mdp_data_at(mdp_in, noreg, constant, decrement);
}

void InterpreterMacroAssembler::increment_mdp_data_at(Register mdp_in,
                                                      Register reg,
                                                      int constant,
                                                      bool decrement) {
  //
}

void InterpreterMacroAssembler::set_mdp_flag_at(Register mdp_in,
                                                int flag_byte_constant) {
                                                  //
}


void InterpreterMacroAssembler::test_mdp_data_at(Register mdp_in,
                                                 int offset,
                                                 Register value,
                                                 Register test_value_out,
                                                 Label& not_equal_continue) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  if (test_value_out == noreg) {
    ldr(rscratch1, Address(mdp_in, offset));
    cmp(value, rscratch1);
  } else {
    // Put the test value into a register, so caller can use it:
    ldr(test_value_out, Address(mdp_in, offset));
    cmp(value, test_value_out);
  }
  br(Assembler::NE, not_equal_continue);
}


void InterpreterMacroAssembler::update_mdp_by_offset(Register mdp_in,
                                                     int offset_of_disp) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  ldr(rscratch1, Address(mdp_in, offset_of_disp));
  add(mdp_in, mdp_in, rscratch1, LSL);
  str(mdp_in, Address(rfp, frame::interpreter_frame_mdx_offset * wordSize));
}


void InterpreterMacroAssembler::update_mdp_by_offset(Register mdp_in,
                                                     Register reg,
                                                     int offset_of_disp) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  lea(rscratch1, Address(mdp_in, offset_of_disp));
  ldr(rscratch1, Address(rscratch1, reg, Address::lsl(0)));
  add(mdp_in, mdp_in, rscratch1, LSL);
  str(mdp_in, Address(rfp, frame::interpreter_frame_mdx_offset * wordSize));
}


void InterpreterMacroAssembler::update_mdp_by_constant(Register mdp_in,
                                                       int constant) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  add(mdp_in, mdp_in, (unsigned)constant);
  str(mdp_in, Address(rfp, frame::interpreter_frame_mdx_offset * wordSize));
}


void InterpreterMacroAssembler::update_mdp_for_ret(Register return_bci) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  // save/restore across call_VM
  stp(zr, return_bci, Address(pre(sp, -2 * wordSize)));
  call_VM(noreg,
          CAST_FROM_FN_PTR(address, InterpreterRuntime::update_mdp_for_ret),
          return_bci);
  ldp(zr, return_bci, Address(post(sp, 2 * wordSize)));
}


void InterpreterMacroAssembler::profile_taken_branch(Register mdp,
                                                     Register bumped_count) {
  if (ProfileInterpreter) {
  
  }
}


void InterpreterMacroAssembler::profile_not_taken_branch(Register mdp) {
  if (ProfileInterpreter) {

  }
}


void InterpreterMacroAssembler::profile_call(Register mdp) { 
  if (ProfileInterpreter) {

  }
}

void InterpreterMacroAssembler::profile_final_call(Register mdp) {
  if (ProfileInterpreter) {

  }
}


void InterpreterMacroAssembler::profile_virtual_call(Register receiver,
                                                     Register mdp,
                                                     Register reg2,
                                                     bool receiver_can_be_null) {
  if (ProfileInterpreter) {

  }
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
  if (ProfileInterpreter) {
 
  }
}

void InterpreterMacroAssembler::profile_null_seen(Register mdp) {
  if (ProfileInterpreter) {
  
  }
}

void InterpreterMacroAssembler::profile_typecheck_failed(Register mdp) {
  if (ProfileInterpreter && TypeProfileCasts) {

  }
}

void InterpreterMacroAssembler::profile_typecheck(Register mdp, Register klass, Register reg2) {
  if (ProfileInterpreter) {

  }
}

void InterpreterMacroAssembler::profile_switch_default(Register mdp) {

}

void InterpreterMacroAssembler::profile_switch_case(Register index,
                                                    Register mdp,
                                                    Register reg2) {
  if (ProfileInterpreter) {

  }
}

void InterpreterMacroAssembler::verify_oop(Register reg, TosState state) {
  if (state == atos) {
    MacroAssembler::verify_oop(reg);
  }
}

void InterpreterMacroAssembler::verify_FPU(int stack_depth, TosState state) { ; }
#endif // !CC_INTERP


void InterpreterMacroAssembler::notify_method_entry() { 
  // Whenever JVMTI is interp_only_mode, method entry/exit events are sent to
  // track stack depth.  If it is possible to enter interp_only_mode we add
  // the code to check if the event should be sent.
 }


void InterpreterMacroAssembler::notify_method_exit(
    TosState state, NotifyMethodExitMode mode) {
  // Whenever JVMTI is interp_only_mode, method entry/exit events are sent to
  // track stack depth.  If it is possible to enter interp_only_mode we add
  // the code to check if the event should be sent.

}


// Jump if ((*counter_addr += increment) & mask) satisfies the condition.
void InterpreterMacroAssembler::increment_mask_and_jump(Address counter_addr,
                                                        int increment, int mask,
                                                        Register scratch, Register scratch2,
							bool preloaded,
                                                        Condition cond, Label* where) {
  if (!preloaded) {
    ldrw(scratch, counter_addr);
  }
  add(scratch, scratch, increment);
  strw(scratch, counter_addr);
  if (operand_valid_for_logical_immediate(/*is32*/true, mask)) {
    andsw(scratch, scratch, mask);
  } else {
    movw(scratch2, (unsigned)mask);
    andsw(scratch, scratch, scratch2);
  }
  br(cond, *where);
}

void InterpreterMacroAssembler::call_VM_leaf_base(address entry_point,
                                                  int number_of_arguments) {
  // interpreter specific
  //
  // Note: No need to save/restore rbcp & rlocals pointer since these
  //       are callee saved registers and no blocking/ GC can happen
  //       in leaf calls.
#ifdef ASSERT
  {
    Label L;
    ldr(rscratch1, Address(rfp, frame::interpreter_frame_last_sp_offset * wordSize));
    cbz(rscratch1, L);
    stop("InterpreterMacroAssembler::call_VM_leaf_base:"
         " last_sp != NULL");
    bind(L);
  }
#endif /* ASSERT */
  // super call
  MacroAssembler::call_VM_leaf_base(entry_point, number_of_arguments);
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
    ldr(rscratch1, Address(rfp, frame::interpreter_frame_last_sp_offset * wordSize));
    cbz(rscratch1, L);
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

void InterpreterMacroAssembler::profile_obj_type(Register obj, const Address& mdo_addr) {

}

void InterpreterMacroAssembler::profile_arguments_type(Register mdp, Register callee, Register tmp, bool is_virtual) {

}

void InterpreterMacroAssembler::profile_return_type(Register mdp, Register ret, Register tmp) {
  assert_different_registers(mdp, ret, tmp, rbcp);

}

void InterpreterMacroAssembler::profile_parameters_type(Register mdp, Register tmp1, Register tmp2) {

}
