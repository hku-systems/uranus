/*
 * Copyright (c) 2013, Red Hat Inc.
 * Copyright (c) 1997, 2012, Oracle and/or its affiliates.
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
#include "interpreter/interpreter.hpp"
#include "memory/resourceArea.hpp"
#include "oops/markOop.hpp"
#include "oops/method.hpp"
#include "oops/oop.inline.hpp"
// #include "prims/methodHandles.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/handles.inline.hpp"
// #include "runtime/javaCalls.hpp"
// #include "runtime/monitorChunk.hpp"
#include "runtime/os.hpp"
#include "runtime/signature.hpp"
#include "runtime/stubCodeGenerator.hpp"
#include "runtime/stubRoutines.hpp"
// #include "vmreg_aarch64.inline.hpp"
#ifdef COMPILER1
#include "c1/c1_Runtime1.hpp"
#include "runtime/vframeArray.hpp"
#endif

#ifdef ASSERT
void RegisterMap::check_location_valid() {
}
#endif


// Profiling/safepoint support

bool frame::safe_for_sender(JavaThread *thread) {
  D_WARN_Unimplement;
  return true;
}

void frame::patch_pc(Thread* thread, address pc) {
  D_WARN_Unimplement;
}

bool frame::is_enclave_frame() const {
    if ((intptr_t)_fp == (intptr_t)(-1) || (intptr_t)_unextended_sp == (intptr_t)-1) {
        printf(D_WARN("Check Frame")" is enclave frame\n");
    }
    return (intptr_t)_fp == (intptr_t)(-1);
}

bool frame::is_interpreted_frame() const  {
  return Interpreter::contains(pc());
}

int frame::frame_size(RegisterMap* map) const {
  frame sender = this->sender(map);
  return sender.sp() - sp();
}

intptr_t* frame::entry_frame_argument_at(int offset) const {
  // convert offset to index to deal with tsi
  int index = (Interpreter::expr_offset_in_bytes(offset)/wordSize);
  // Entry frame's arguments are always in relation to unextended_sp()
  return &unextended_sp()[index];
}

// sender_sp
#ifdef CC_INTERP
intptr_t* frame::interpreter_frame_sender_sp() const {
  assert(is_interpreted_frame(), "interpreted frame expected");
  // QQQ why does this specialize method exist if frame::sender_sp() does same thing?
  // seems odd and if we always know interpreted vs. non then sender_sp() is really
  // doing too much work.
  return get_interpreterState()->sender_sp();
}

// monitor elements

BasicObjectLock* frame::interpreter_frame_monitor_begin() const {
  return get_interpreterState()->monitor_base();
}

BasicObjectLock* frame::interpreter_frame_monitor_end() const {
  return (BasicObjectLock*) get_interpreterState()->stack_base();
}

#else // CC_INTERP

intptr_t* frame::interpreter_frame_sender_sp() const {
  assert(is_interpreted_frame(), "interpreted frame expected");
  return (intptr_t*) at(interpreter_frame_sender_sp_offset);
}

void frame::set_interpreter_frame_sender_sp(intptr_t* sender_sp) {
  assert(is_interpreted_frame(), "interpreted frame expected");
  ptr_at_put(interpreter_frame_sender_sp_offset, (intptr_t) sender_sp);
}


// monitor elements

BasicObjectLock* frame::interpreter_frame_monitor_begin() const {
  return (BasicObjectLock*) addr_at(interpreter_frame_monitor_block_bottom_offset);
}

BasicObjectLock* frame::interpreter_frame_monitor_end() const {
  BasicObjectLock* result = (BasicObjectLock*) *addr_at(interpreter_frame_monitor_block_top_offset);
  // make sure the pointer points inside the frame
  assert(sp() <= (intptr_t*) result, "monitor end should be above the stack pointer");
  assert((intptr_t*) result < fp(),  "monitor end should be strictly below the frame pointer");
  return result;
}

void frame::interpreter_frame_set_monitor_end(BasicObjectLock* value) {
  *((BasicObjectLock**)addr_at(interpreter_frame_monitor_block_top_offset)) = value;
}

// Used by template based interpreter deoptimization
void frame::interpreter_frame_set_last_sp(intptr_t* sp) {
    *((intptr_t**)addr_at(interpreter_frame_last_sp_offset)) = sp;
}
#endif // CC_INTERP

frame frame::sender_for_entry_frame(RegisterMap* map) const {
  ShouldNotReachHere();
}

//------------------------------------------------------------------------------
// frame::verify_deopt_original_pc
//
// Verifies the calculated original PC of a deoptimization PC for the
// given unextended SP.  The unextended SP might also be the saved SP
// for MethodHandle call sites.
#ifdef ASSERT
void frame::verify_deopt_original_pc(nmethod* nm, intptr_t* unextended_sp, bool is_method_handle_return) {
  frame fr;

  // This is ugly but it's better than to change {get,set}_original_pc
  // to take an SP value as argument.  And it's only a debugging
  // method anyway.
  fr._unextended_sp = unextended_sp;

  address original_pc = nm->get_original_pc(&fr);
  assert(nm->insts_contains(original_pc), "original PC must be in nmethod");
  assert(nm->is_method_handle_return(original_pc) == is_method_handle_return, "must be");
}
#endif

//------------------------------------------------------------------------------
// frame::adjust_unextended_sp
void frame::adjust_unextended_sp() {
  // If we are returning to a compiled MethodHandle call site, the
  // saved_fp will in fact be a saved value of the unextended SP.  The
  // simplest way to tell whether we are returning to such a call site
  // is as follows:
}

//------------------------------------------------------------------------------
// frame::update_map_with_saved_link
void frame::update_map_with_saved_link(RegisterMap* map, intptr_t** link_addr) {
  D_WARN_Unimplement;
}


//------------------------------------------------------------------------------
// frame::sender_for_interpreter_frame
frame frame::sender_for_interpreter_frame(RegisterMap* map) const {
  // SP is the raw SP from the sender after adapter or interpreter
  // extension.
  intptr_t* sender_sp = this->sender_sp();

  // This is the sp before any possible extension (adapter/locals).
  intptr_t* unextended_sp = interpreter_frame_sender_sp();

#ifdef COMPILER2
  if (map->update_map()) {
    update_map_with_saved_link(map, (intptr_t**) addr_at(link_offset));
  }
#endif // COMPILER2

  return frame(sender_sp, unextended_sp, link(), sender_pc());
}


//------------------------------------------------------------------------------
// frame::sender_for_compiled_frame
frame frame::sender_for_compiled_frame(RegisterMap* map) const {
  ShouldNotReachHere();
}

//------------------------------------------------------------------------------
// frame::sender
frame frame::sender(RegisterMap* map) const {
  // Default is we done have to follow them. The sender_for_xxx will
  // update it accordingly
   map->set_include_argument_oops(false);

  if (is_entry_frame())
    return sender_for_entry_frame(map);
  if (is_interpreted_frame())
    return sender_for_interpreter_frame(map);
  assert(_cb == CodeCache::find_blob(pc()),"Must be the same");

  // This test looks odd: why is it not is_compiled_frame() ?  That's
  // because stubs also have OOP maps.
  if (_cb != NULL) {
    return sender_for_compiled_frame(map);
  }

  // Must be native-compiled frame, i.e. the marshaling code for native
  // methods that exists in the core system.
  return frame(sender_sp(), link(), sender_pc());
}

bool frame::interpreter_frame_equals_unpacked_fp(intptr_t* fp) {
  assert(is_interpreted_frame(), "must be interpreter frame");
  Method* method = interpreter_frame_method();
  // When unpacking an optimized frame the frame pointer is
  // adjusted with:
  int diff = (method->max_locals() - method->size_of_parameters()) *
             Interpreter::stackElementWords;
  return _fp == (fp - diff);
}

void frame::pd_gc_epilog() {
  // nothing done here now
}

bool frame::is_interpreted_frame_valid(JavaThread* thread) const {
// QQQ
  D_WARN_Unimplement;
  return true;
}

BasicType frame::interpreter_frame_result(oop* oop_result, jvalue* value_result) {
#ifdef CC_INTERP
  // Needed for JVMTI. The result should always be in the
  // interpreterState object
  interpreterState istate = get_interpreterState();
#endif // CC_INTERP
  assert(is_interpreted_frame(), "interpreted frame expected");
  Method* method = interpreter_frame_method();
  BasicType type = method->result_type();

  intptr_t* tos_addr;
  if (method->is_native()) {
    // TODO : ensure AARCH64 does the same as Intel here i.e. push v0 then r0
    // Prior to calling into the runtime to report the method_exit the possible
    // return value is pushed to the native stack. If the result is a jfloat/jdouble
    // then ST0 is saved before EAX/EDX. See the note in generate_native_result
    tos_addr = (intptr_t*)sp();
    if (type == T_FLOAT || type == T_DOUBLE) {
      // This is times two because we do a push(ltos) after pushing XMM0
      // and that takes two interpreter stack slots.
      tos_addr += 2 * Interpreter::stackElementWords;
    }
  } else {
    tos_addr = (intptr_t*)interpreter_frame_tos_address();
  }

  switch (type) {
    case T_OBJECT  :
    case T_ARRAY   : {
      oop obj;
      if (method->is_native()) {
#ifdef CC_INTERP
        obj = istate->_oop_temp;
#else
        obj = cast_to_oop(at(interpreter_frame_oop_temp_offset));
#endif // CC_INTERP
      } else {
        oop* obj_p = (oop*)tos_addr;
        obj = (obj_p == NULL) ? (oop)NULL : *obj_p;
      }
      assert(obj == NULL || Universe::heap()->is_in(obj), "sanity check");
      *oop_result = obj;
      break;
    }
    case T_BOOLEAN : value_result->z = *(jboolean*)tos_addr; break;
    case T_BYTE    : value_result->b = *(jbyte*)tos_addr; break;
    case T_CHAR    : value_result->c = *(jchar*)tos_addr; break;
    case T_SHORT   : value_result->s = *(jshort*)tos_addr; break;
    case T_INT     : value_result->i = *(jint*)tos_addr; break;
    case T_LONG    : value_result->j = *(jlong*)tos_addr; break;
    case T_FLOAT   : {
        value_result->f = *(jfloat*)tos_addr;
      break;
    }
    case T_DOUBLE  : value_result->d = *(jdouble*)tos_addr; break;
    case T_VOID    : /* Nothing to do */ break;
    default        : ShouldNotReachHere();
  }

  return type;
}


intptr_t* frame::interpreter_frame_tos_at(jint offset) const {
  int index = (Interpreter::expr_offset_in_bytes(offset)/wordSize);
  return &interpreter_frame_tos_address()[index];
}

#ifndef PRODUCT

#define DESCRIBE_FP_OFFSET(name) \
  values.describe(frame_no, fp() + frame::name##_offset, #name)

void frame::describe_pd(FrameValues& values, int frame_no) {
  if (is_interpreted_frame()) {
    DESCRIBE_FP_OFFSET(interpreter_frame_sender_sp);
    DESCRIBE_FP_OFFSET(interpreter_frame_last_sp);
    DESCRIBE_FP_OFFSET(interpreter_frame_method);
    DESCRIBE_FP_OFFSET(interpreter_frame_mdx);
    DESCRIBE_FP_OFFSET(interpreter_frame_cache);
    DESCRIBE_FP_OFFSET(interpreter_frame_locals);
    DESCRIBE_FP_OFFSET(interpreter_frame_bcx);
    DESCRIBE_FP_OFFSET(interpreter_frame_initial_sp);
  }
}
#endif

intptr_t *frame::initial_deoptimization_info() {
  // Not used on aarch64, but we must return something.
  return NULL;
}

intptr_t* frame::real_fp() const {
  if (_cb != NULL) {
    // use the frame size if valid
    int size = _cb->frame_size();
    if (size > 0) {
      return unextended_sp() + size;
    }
  }
  // else rely on fp()
  assert(! is_compiled_frame(), "unknown compiled frame size");
  return fp();
}

#undef DESCRIBE_FP_OFFSET

#define DESCRIBE_FP_OFFSET(name)					\
  {									\
    unsigned long *p = (unsigned long *)fp;				\
    printf("0x%016lx 0x%016lx %s\n", (unsigned long)(p + frame::name##_offset), \
	   p[frame::name##_offset], #name);				\
  }

static __thread unsigned long nextfp;
static __thread unsigned long nextpc;
static __thread unsigned long nextsp;
static __thread RegisterMap *reg_map;

static void printbc(Method *m, intptr_t bcx) {
  const char *name;
  char buf[16];
  if (m->validate_bci_from_bcx(bcx) < 0
      || !m->contains((address)bcx)) {
    name = "???";
    snprintf(buf, sizeof buf, "(bad)");
  } else {
    int bci = m->bci_from((address)bcx);
    snprintf(buf, sizeof buf, "%d", bci);
    name = Bytecodes::name(m->code_at(bci));
  }
  ResourceMark rm;
  printf("%s : %s ==> %s\n", m->name_and_sig_as_C_string(), buf, name);
}

void internal_pf(unsigned long sp, unsigned long fp, unsigned long pc, unsigned long bcx) {
  if (! fp)
    return;

  DESCRIBE_FP_OFFSET(return_addr);
  DESCRIBE_FP_OFFSET(link);
  DESCRIBE_FP_OFFSET(interpreter_frame_sender_sp);
  DESCRIBE_FP_OFFSET(interpreter_frame_last_sp);
  DESCRIBE_FP_OFFSET(interpreter_frame_method);
  DESCRIBE_FP_OFFSET(interpreter_frame_mdx);
  DESCRIBE_FP_OFFSET(interpreter_frame_cache);
  DESCRIBE_FP_OFFSET(interpreter_frame_locals);
  DESCRIBE_FP_OFFSET(interpreter_frame_bcx);
  DESCRIBE_FP_OFFSET(interpreter_frame_initial_sp);
  unsigned long *p = (unsigned long *)fp;

  // We want to see all frames, native and Java.  For compiled and
  // interpreted frames we have special information that allows us to
  // unwind them; for everything else we assume that the native frame
  // pointer chain is intact.
  frame this_frame((intptr_t*)sp, (intptr_t*)fp, (address)pc);
  if (this_frame.is_compiled_frame() ||
      this_frame.is_interpreted_frame()) {
    frame sender = this_frame.sender(reg_map);
    nextfp = (unsigned long)sender.fp();
    nextpc = (unsigned long)sender.pc();
    nextsp = (unsigned long)sender.unextended_sp();
  } else {
    nextfp = p[frame::link_offset];
    nextpc = p[frame::return_addr_offset];
    nextsp = (unsigned long)&p[frame::sender_sp_offset];
  }

  if (bcx == -1ul)
    bcx = p[frame::interpreter_frame_bcx_offset];

  if (Interpreter::contains((address)pc)) {
    Method* m = (Method*)p[frame::interpreter_frame_method_offset];
    if(m && m->is_method()) {
      printbc(m, bcx);
    } else
      printf("not a Method\n");
  } else {
    D_WARN_Unimplement;
  }
}

extern "C" void npf() {
  CodeBlob *cb = NULL;
  // C2 does not always chain the frame pointers when it can, instead
  // preferring to use fixed offsets from SP, so a simple leave() does
  // not work.  Instead, it adds the frame size to SP then pops FP and
  // LR.  We have to do the same thing to get a good call chain.
  if (cb && cb->frame_size())
    nextfp = nextsp + wordSize * (cb->frame_size() - 2);
  internal_pf (nextsp, nextfp, nextpc, -1);
}

extern "C" void pf(unsigned long sp, unsigned long fp, unsigned long pc,
		   unsigned long bcx, unsigned long thread) {
  RegisterMap map((JavaThread*)thread, false);
  if (!reg_map) {
    reg_map = (RegisterMap*)os::malloc(sizeof map, mtNone);
  }
  memcpy(reg_map, &map, sizeof map);
  {
    CodeBlob *cb = NULL;
    if (cb && cb->frame_size())
      fp = sp + wordSize * (cb->frame_size() - 2);
  }
  internal_pf(sp, fp, pc, bcx);
}

// support for printing out where we are in a Java method
// needs to be passed current fp and bcp register values
// prints method name, bc index and bytecode name
extern "C" void pm(unsigned long fp, unsigned long bcx) {
  DESCRIBE_FP_OFFSET(interpreter_frame_method);
  unsigned long *p = (unsigned long *)fp;
  Method* m = (Method*)p[frame::interpreter_frame_method_offset];
  printbc(m, bcx);
}

#ifndef PRODUCT
// This is a generic constructor which is only used by pns() in debug.cpp.
frame::frame(void* sp, void* fp, void* pc) {
  init((intptr_t*)sp, (intptr_t*)fp, (address)pc);
}
#endif

void JavaFrameAnchor::make_walkable(JavaThread* thread) {
  // last frame set?
  if (last_Java_sp() == NULL) return;
  // already walkable?
  if (walkable()) return;
  assert(Thread::current() == (Thread*)thread, "not current thread");
  assert(last_Java_sp() != NULL, "not called from Java code?");
  assert(last_Java_pc() == NULL, "already walkable");
  capture_last_Java_pc();
  assert(walkable(), "something went wrong");
}

void JavaFrameAnchor::capture_last_Java_pc() {
  assert(_last_Java_sp != NULL, "no last frame set");
  assert(_last_Java_pc == NULL, "already walkable");
  _last_Java_pc = (address)_last_Java_sp[-1];
}
