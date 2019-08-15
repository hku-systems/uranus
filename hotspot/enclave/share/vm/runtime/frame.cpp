/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include "interpreter/oopMapCache.hpp"
#include "memory/resourceArea.hpp"
#include "memory/universe.inline.hpp"
#include "oops/markOop.hpp"
#include "oops/method.hpp"
#include "oops/oop.inline.hpp"
#include "oops/oop.inline2.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/signature.hpp"
#include "runtime/stubCodeGenerator.hpp"

#ifdef TARGET_ARCH_x86
# include "nativeInst_x86.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "nativeInst_sparc.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "nativeInst_zero.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "nativeInst_arm.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "nativeInst_ppc.hpp"
#endif

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

RegisterMap::RegisterMap(JavaThread *thread, bool update_map) {
  _thread         = thread;
  _update_map     = update_map;
  clear();
  debug_only(_update_for_id = NULL;)
#ifndef PRODUCT
  for (int i = 0; i < reg_count ; i++ ) _location[i] = NULL;
#endif /* PRODUCT */
}

RegisterMap::RegisterMap(const RegisterMap* map) {
  assert(map != this, "bad initialization parameter");
  assert(map != NULL, "RegisterMap must be present");
  _thread                = map->thread();
  _update_map            = map->update_map();
  _include_argument_oops = map->include_argument_oops();
  debug_only(_update_for_id = map->_update_for_id;)
  pd_initialize_from(map);
  if (update_map()) {
    for(int i = 0; i < location_valid_size; i++) {
      LocationValidType bits = !update_map() ? 0 : map->_location_valid[i];
      _location_valid[i] = bits;
      // for whichever bits are set, pull in the corresponding map->_location
      int j = i*location_valid_type_size;
      while (bits != 0) {
        if ((bits & 1) != 0) {
          assert(0 <= j && j < reg_count, "range check");
          _location[j] = map->_location[j];
        }
        bits >>= 1;
        j += 1;
      }
    }
  }
}

void RegisterMap::clear() {
  set_include_argument_oops(true);
  if (_update_map) {
    for(int i = 0; i < location_valid_size; i++) {
      _location_valid[i] = 0;
    }
    pd_clear();
  } else {
    pd_initialize();
  }
}

#ifndef PRODUCT

void RegisterMap::print_on(outputStream* st) const {
  D_WARN_Unimplement;
}

void RegisterMap::print() const {
  print_on(tty);
}

#endif
// This returns the pc that if you were in the debugger you'd see. Not
// the idealized value in the frame object. This undoes the magic conversion
// that happens for deoptimized frames. In addition it makes the value the
// hardware would want to see in the native frame. The only user (at this point)
// is deoptimization. It likely no one else should ever use it.

address frame::raw_pc() const {
  return (pc() - pc_return_offset);
}

// Change the pc in a frame object. This does not change the actual pc in
// actual frame. To do that use patch_pc.
//
void frame::set_pc(address   newpc ) {
  D_WARN_Unimplement;
}

// type testers
bool frame::is_ignored_frame() const {
  return false;  // FIXME: some LambdaForm frames should be ignored
}
bool frame::is_deoptimized_frame() const {
  assert(_deopt_state != unknown, "not answerable");
  return _deopt_state == is_deoptimized;
}

bool frame::is_native_frame() const {
  return false;
}

bool frame::is_java_frame() const {
  if (is_interpreted_frame()) return true;
  if (is_compiled_frame())    return true;
  return false;
}


bool frame::is_compiled_frame() const {
  return false;
}


bool frame::is_runtime_frame() const {
  return (_cb != NULL && _cb->is_runtime_stub());
}

bool frame::is_safepoint_blob_frame() const {
  return (_cb != NULL && _cb->is_safepoint_stub());
}

// testers

bool frame::is_first_java_frame() const {
  RegisterMap map(JavaThread::current(), false); // No update
  frame s;
  for (s = sender(&map); !(s.is_java_frame() || s.is_first_frame()); s = s.sender(&map));
  return s.is_first_frame();
}


bool frame::entry_frame_is_first() const {
  D_WARN_Unimplement;
  return false;
}

JavaCallWrapper* frame::entry_frame_call_wrapper_if_safe(JavaThread* thread) const {
  D_WARN_Unimplement;
}

bool frame::should_be_deoptimized() const {
  return false;
}

bool frame::can_be_deoptimized() const {
  return false;
}

void frame::deoptimize(JavaThread* thread) {
  D_WARN_Unimplement;
}

frame frame::java_sender() const {
  RegisterMap map(JavaThread::current(), false);
  frame s;
  for (s = sender(&map); !(s.is_java_frame() || s.is_first_frame()); s = s.sender(&map)) ;
  guarantee(s.is_java_frame(), "tried to get caller of first java frame");
  return s;
}

frame frame::real_sender(RegisterMap* map) const {
  frame result = sender(map);
  while (result.is_runtime_frame() ||
         result.is_ignored_frame()) {
    result = result.sender(map);
  }
  return result;
}

// Note: called by profiler - NOT for current thread
frame frame::profile_find_Java_sender_frame(JavaThread *thread) {
// If we don't recognize this frame, walk back up the stack until we do
  RegisterMap map(thread, false);
  frame first_java_frame = frame();

  // Find the first Java frame on the stack starting with input frame
  if (is_java_frame()) {
    // top frame is compiled frame or deoptimized frame
    first_java_frame = *this;
  } else if (safe_for_sender(thread)) {
    for (frame sender_frame = sender(&map);
      sender_frame.safe_for_sender(thread) && !sender_frame.is_first_frame();
      sender_frame = sender_frame.sender(&map)) {
      if (sender_frame.is_java_frame()) {
        first_java_frame = sender_frame;
        break;
      }
    }
  }
  return first_java_frame;
}

// Interpreter frames


void frame::interpreter_frame_set_locals(intptr_t* locs)  {
  assert(is_interpreted_frame(), "Not an interpreted frame");
  *interpreter_frame_locals_addr() = locs;
}

Method* frame::interpreter_frame_method() const {
  assert(is_interpreted_frame(), "interpreted frame expected");
  Method* m = *interpreter_frame_method_addr();
  assert(m->is_method(), "not a Method*");
  return m;
}

void frame::interpreter_frame_set_method(Method* method) {
  assert(is_interpreted_frame(), "interpreted frame expected");
  *interpreter_frame_method_addr() = method;
}

void frame::interpreter_frame_set_bcx(intptr_t bcx) {
  *interpreter_frame_bcx_addr() = bcx;
}

jint frame::interpreter_frame_bci() const {
  assert(is_interpreted_frame(), "interpreted frame expected");
  intptr_t bcx = interpreter_frame_bcx();
  return is_bci(bcx) ? bcx : interpreter_frame_method()->bci_from((address)bcx);
}

void frame::interpreter_frame_set_bci(jint bci) {
  assert(is_interpreted_frame(), "interpreted frame expected");
  assert(!is_bci(interpreter_frame_bcx()), "should not set bci during GC");
  interpreter_frame_set_bcx((intptr_t)interpreter_frame_method()->bcp_from(bci));
}

address frame::interpreter_frame_bcp() const {
  assert(is_interpreted_frame(), "interpreted frame expected");
  intptr_t bcx = interpreter_frame_bcx();
  return is_bci(bcx) ? interpreter_frame_method()->bcp_from(bcx) : (address)bcx;
}

void frame::interpreter_frame_set_bcp(address bcp) {
  assert(is_interpreted_frame(), "interpreted frame expected");
  assert(!is_bci(interpreter_frame_bcx()), "should not set bcp during GC");
  interpreter_frame_set_bcx((intptr_t)bcp);
}

void frame::interpreter_frame_set_mdx(intptr_t mdx) {
  assert(is_interpreted_frame(), "Not an interpreted frame");
  assert(ProfileInterpreter, "must be profiling interpreter");
  *interpreter_frame_mdx_addr() = mdx;
}

address frame::interpreter_frame_mdp() const {
  assert(ProfileInterpreter, "must be profiling interpreter");
  assert(is_interpreted_frame(), "interpreted frame expected");
  intptr_t bcx = interpreter_frame_bcx();
  intptr_t mdx = interpreter_frame_mdx();

  assert(!is_bci(bcx), "should not access mdp during GC");
  return (address)mdx;
}

void frame::interpreter_frame_set_mdp(address mdp) {
  assert(is_interpreted_frame(), "interpreted frame expected");
  if (mdp == NULL) {
    // Always allow the mdp to be cleared.
    interpreter_frame_set_mdx((intptr_t)mdp);
  }
  intptr_t bcx = interpreter_frame_bcx();
  assert(!is_bci(bcx), "should not set mdp during GC");
  interpreter_frame_set_mdx((intptr_t)mdp);
}

BasicObjectLock* frame::next_monitor_in_interpreter_frame(BasicObjectLock* current) const {
  assert(is_interpreted_frame(), "Not an interpreted frame");
#ifdef ASSERT
  interpreter_frame_verify_monitor(current);
#endif
  BasicObjectLock* next = (BasicObjectLock*) (((intptr_t*) current) + interpreter_frame_monitor_size());
  return next;
}

BasicObjectLock* frame::previous_monitor_in_interpreter_frame(BasicObjectLock* current) const {
  assert(is_interpreted_frame(), "Not an interpreted frame");
#ifdef ASSERT
//   // This verification needs to be checked before being enabled
//   interpreter_frame_verify_monitor(current);
#endif
  BasicObjectLock* previous = (BasicObjectLock*) (((intptr_t*) current) - interpreter_frame_monitor_size());
  return previous;
}

// Interpreter locals and expression stack locations.

intptr_t* frame::interpreter_frame_local_at(int index) const {
  const int n = Interpreter::local_offset_in_bytes(index)/wordSize;
  return &((*interpreter_frame_locals_addr())[n]);
}

intptr_t* frame::interpreter_frame_expression_stack_at(jint offset) const {
  const int i = offset * interpreter_frame_expression_stack_direction();
  const int n = i * Interpreter::stackElementWords;
  return &(interpreter_frame_expression_stack()[n]);
}

jint frame::interpreter_frame_expression_stack_size() const {
  // Number of elements on the interpreter expression stack
  // Callers should span by stackElementWords
  int element_size = Interpreter::stackElementWords;
  size_t stack_size = 0;
  if (frame::interpreter_frame_expression_stack_direction() < 0) {
    stack_size = (interpreter_frame_expression_stack() -
                  interpreter_frame_tos_address() + 1)/element_size;
  } else {
    stack_size = (interpreter_frame_tos_address() -
                  interpreter_frame_expression_stack() + 1)/element_size;
  }
  assert( stack_size <= (size_t)max_jint, "stack size too big");
  return ((jint)stack_size);
}


// (frame::interpreter_frame_sender_sp accessor is in frame_<arch>.cpp)

const char* frame::print_name() const {
  if (is_native_frame())      return "Native";
  if (is_interpreted_frame()) return "Interpreted";
  if (is_compiled_frame()) {
    if (is_deoptimized_frame()) return "Deoptimized";
    return "Compiled";
  }
  if (sp() == NULL)            return "Empty";
  return "C";
}

void frame::print_value_on(outputStream* st, JavaThread *thread) const {
  //
}


void frame::print_on(outputStream* st) const {
  print_value_on(st,NULL);
  if (is_interpreted_frame()) {
    interpreter_frame_print_on(st);
  }
}


void frame::interpreter_frame_print_on(outputStream* st) const {
//
}

// Return whether the frame is in the VM or os indicating a Hotspot problem.
// Otherwise, it's likely a bug in the native library that the Java code calls,
// hopefully indicating where to submit bugs.
void frame::print_C_frame(outputStream* st, char* buf, int buflen, address pc) {
//
}

// frame::print_on_error() is called by fatal error handler. Notice that we may
// crash inside this function if stack frame is corrupted. The fatal error
// handler can catch and handle the crash. Here we assume the frame is valid.
//
// First letter indicates type of the frame:
//    J: Java frame (compiled)
//    j: Java frame (interpreted)
//    V: VM frame (C/C++)
//    v: Other frames running VM generated code (e.g. stubs, adapters, etc.)
//    C: C/C++ frame
//
// We don't need detailed frame type as that in frame::print_name(). "C"
// suggests the problem is in user lib; everything else is likely a VM bug.

void frame::print_on_error(outputStream* st, char* buf, int buflen, bool verbose) const {
//
}


/*
  The interpreter_frame_expression_stack_at method in the case of SPARC needs the
  max_stack value of the method in order to compute the expression stack address.
  It uses the Method* in order to get the max_stack value but during GC this
  Method* value saved on the frame is changed by reverse_and_push and hence cannot
  be used. So we save the max_stack value in the FrameClosure object and pass it
  down to the interpreter_frame_expression_stack_at method
*/
class InterpreterFrameClosure : public OffsetClosure {
 private:
  frame* _fr;
  OopClosure* _f;
  int    _max_locals;
  int    _max_stack;

 public:
  InterpreterFrameClosure(frame* fr, int max_locals, int max_stack,
                          OopClosure* f) {
    _fr         = fr;
    _max_locals = max_locals;
    _max_stack  = max_stack;
    _f          = f;
  }

  void offset_do(int offset) {
    oop* addr;
    if (offset < _max_locals) {
      addr = (oop*) _fr->interpreter_frame_local_at(offset);
      assert((intptr_t*)addr >= _fr->sp(), "must be inside the frame");
      _f->do_oop(addr);
    } else {
      addr = (oop*) _fr->interpreter_frame_expression_stack_at((offset - _max_locals));
      // In case of exceptions, the expression stack is invalid and the esp will be reset to express
      // this condition. Therefore, we call f only if addr is 'inside' the stack (i.e., addr >= esp for Intel).
      bool in_stack;
      if (frame::interpreter_frame_expression_stack_direction() > 0) {
        in_stack = (intptr_t*)addr <= _fr->interpreter_frame_tos_address();
      } else {
        in_stack = (intptr_t*)addr >= _fr->interpreter_frame_tos_address();
      }
      if (in_stack) {
        _f->do_oop(addr);
      }
    }
  }

  int max_locals()  { return _max_locals; }
  frame* fr()       { return _fr; }
};


class InterpretedArgumentOopFinder: public SignatureInfo {
 private:
  OopClosure* _f;        // Closure to invoke
  int    _offset;        // TOS-relative offset, decremented with each argument
  bool   _has_receiver;  // true if the callee has a receiver
  frame* _fr;

  void set(int size, BasicType type) {
    _offset -= size;
    if (type == T_OBJECT || type == T_ARRAY) oop_offset_do();
  }

  void oop_offset_do() {
    oop* addr;
    addr = (oop*)_fr->interpreter_frame_tos_at(_offset);
    _f->do_oop(addr);
  }

 public:
  InterpretedArgumentOopFinder(Symbol* signature, bool has_receiver, frame* fr, OopClosure* f) : SignatureInfo(signature), _has_receiver(has_receiver) {
    // compute size of arguments
    int args_size = ArgumentSizeComputer(signature).size() + (has_receiver ? 1 : 0);
    assert(!fr->is_interpreted_frame() ||
           args_size <= fr->interpreter_frame_expression_stack_size(),
            "args cannot be on stack anymore");
    // initialize InterpretedArgumentOopFinder
    _f         = f;
    _fr        = fr;
    _offset    = args_size;
  }

  void oops_do() {
    if (_has_receiver) {
      --_offset;
      oop_offset_do();
    }
    iterate_parameters();
  }
};


// Entry frame has following form (n arguments)
//         +-----------+
//   sp -> |  last arg |
//         +-----------+
//         :    :::    :
//         +-----------+
// (sp+n)->|  first arg|
//         +-----------+



// visits and GC's all the arguments in entry frame
class EntryFrameOopFinder: public SignatureInfo {
 private:
  bool   _is_static;
  int    _offset;
  frame* _fr;
  OopClosure* _f;

  void set(int size, BasicType type) {
    assert (_offset >= 0, "illegal offset");
    if (type == T_OBJECT || type == T_ARRAY) oop_at_offset_do(_offset);
    _offset -= size;
  }

  void oop_at_offset_do(int offset) {
    assert (offset >= 0, "illegal offset");
    oop* addr = (oop*) _fr->entry_frame_argument_at(offset);
    _f->do_oop(addr);
  }

 public:
   EntryFrameOopFinder(frame* frame, Symbol* signature, bool is_static) : SignatureInfo(signature) {
     _f = NULL; // will be set later
     _fr = frame;
     _is_static = is_static;
     _offset = ArgumentSizeComputer(signature).size() - 1; // last parameter is at index 0
   }

  void arguments_do(OopClosure* f) {
    _f = f;
    if (!_is_static) oop_at_offset_do(_offset+1); // do the receiver
    iterate_parameters();
  }

};

oop* frame::interpreter_callee_receiver_addr(Symbol* signature) {
  ArgumentSizeComputer asc(signature);
  int size = asc.size();
  return (oop *)interpreter_frame_tos_at(size);
}


void frame::oops_interpreted_do(OopClosure* f, CLDClosure* cld_f,
    const RegisterMap* map, bool query_oop_map_cache) {
 D_WARN_Unimplement;
}


void frame::oops_interpreted_arguments_do(Symbol* signature, bool has_receiver, OopClosure* f) {
  InterpretedArgumentOopFinder finder(signature, has_receiver, this, f);
  finder.oops_do();
}

void frame::oops_code_blob_do(OopClosure* f, CodeBlobClosure* cf, const RegisterMap* reg_map) {
  D_WARN_Unimplement;
}

void frame::oops_compiled_arguments_do(Symbol* signature, bool has_receiver, bool has_appendix, const RegisterMap* reg_map, OopClosure* f) {
  D_WARN_Unimplement;
}


// Get receiver out of callers frame, i.e. find parameter 0 in callers
// frame.  Consult ADLC for where parameter 0 is to be found.  Then
// check local reg_map for it being a callee-save register or argument
// register, both of which are saved in the local frame.  If not found
// there, it must be an in-stack argument of the caller.
// Note: caller.sp() points to callee-arguments
oop frame::retrieve_receiver(RegisterMap* reg_map) {
  D_WARN_Unimplement;
}


oop* frame::oopmapreg_to_location(VMReg reg, const RegisterMap* reg_map) const {
  D_WARN_Unimplement;
}

BasicLock* frame::get_native_monitor() {
  D_WARN_Unimplement;
}

oop frame::get_native_receiver() {
  D_WARN_Unimplement;
}

void frame::oops_entry_do(OopClosure* f, const RegisterMap* map) {
  D_WARN_Unimplement;
}


void frame::oops_do_internal(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf, RegisterMap* map, bool use_interpreter_oop_map_cache) {
  D_WARN_Unimplement;
}

void frame::nmethods_do(CodeBlobClosure* cf) {
  if (_cb != NULL && _cb->is_nmethod()) {
    cf->do_code_blob(_cb);
  }
}


// call f() on the interpreted Method*s in the stack.
// Have to walk the entire code cache for the compiled frames Yuck.
void frame::metadata_do(void f(Metadata*)) {
  if (_cb != NULL && Interpreter::contains(pc())) {
    Method* m = this->interpreter_frame_method();
    assert(m != NULL, "huh?");
    f(m);
  }
}

void frame::gc_prologue() {
  if (is_interpreted_frame()) {
    // set bcx to bci to become Method* position independent during GC
    interpreter_frame_set_bcx(interpreter_frame_bci());
  }
}


void frame::gc_epilogue() {
  if (is_interpreted_frame()) {
    // set bcx back to bcp for interpreter
    interpreter_frame_set_bcx((intptr_t)interpreter_frame_bcp());
  }
  // call processor specific epilog function
  pd_gc_epilog();
}


# ifdef ENABLE_ZAP_DEAD_LOCALS

void frame::CheckValueClosure::do_oop(oop* p) {
  if (CheckOopishValues && Universe::heap()->is_in_reserved(*p)) {
    warning("value @ " INTPTR_FORMAT " looks oopish (" INTPTR_FORMAT ") (thread = " INTPTR_FORMAT ")", p, (address)*p, Thread::current());
  }
}
frame::CheckValueClosure frame::_check_value;


void frame::CheckOopClosure::do_oop(oop* p) {
  if (*p != NULL && !(*p)->is_oop()) {
    warning("value @ " INTPTR_FORMAT " should be an oop (" INTPTR_FORMAT ") (thread = " INTPTR_FORMAT ")", p, (address)*p, Thread::current());
 }
}
frame::CheckOopClosure frame::_check_oop;

void frame::check_derived_oop(oop* base, oop* derived) {
  _check_oop.do_oop(base);
}


void frame::ZapDeadClosure::do_oop(oop* p) {
  if (TraceZapDeadLocals) tty->print_cr("zapping @ " INTPTR_FORMAT " containing " INTPTR_FORMAT, p, (address)*p);
  *p = cast_to_oop<intptr_t>(0xbabebabe);
}
frame::ZapDeadClosure frame::_zap_dead;

void frame::zap_dead_locals(JavaThread* thread, const RegisterMap* map) {
  assert(thread == Thread::current(), "need to synchronize to do this to another thread");
  // Tracing - part 1
  if (TraceZapDeadLocals) {
    ResourceMark rm(thread);
    tty->print_cr("--------------------------------------------------------------------------------");
    tty->print("Zapping dead locals in ");
    print_on(tty);
    tty->cr();
  }
  // Zapping
       if (is_entry_frame      ()) zap_dead_entry_locals      (thread, map);
  else if (is_interpreted_frame()) zap_dead_interpreted_locals(thread, map);
  else if (is_compiled_frame()) zap_dead_compiled_locals   (thread, map);

  else
    // could be is_runtime_frame
    // so remove error: ShouldNotReachHere();
    ;
  // Tracing - part 2
  if (TraceZapDeadLocals) {
    tty->cr();
  }
}


void frame::zap_dead_interpreted_locals(JavaThread *thread, const RegisterMap* map) {
  // get current interpreter 'pc'
  assert(is_interpreted_frame(), "Not an interpreted frame");
  Method* m   = interpreter_frame_method();
  int       bci = interpreter_frame_bci();

  int max_locals = m->is_native() ? m->size_of_parameters() : m->max_locals();

  // process dynamic part
  InterpreterFrameClosure value_blk(this, max_locals, m->max_stack(),
                                    &_check_value);
  InterpreterFrameClosure   oop_blk(this, max_locals, m->max_stack(),
                                    &_check_oop  );
  InterpreterFrameClosure  dead_blk(this, max_locals, m->max_stack(),
                                    &_zap_dead   );

  // get frame map
  InterpreterOopMap mask;
  m->mask_for(bci, &mask);
  mask.iterate_all( &oop_blk, &value_blk, &dead_blk);
}


void frame::zap_dead_compiled_locals(JavaThread* thread, const RegisterMap* reg_map) {

  ResourceMark rm(thread);
  assert(_cb != NULL, "sanity check");
  if (_cb->oop_maps() != NULL) {
    OopMapSet::all_do(this, reg_map, &_check_oop, check_derived_oop, &_check_value);
  }
}


void frame::zap_dead_entry_locals(JavaThread*, const RegisterMap*) {
  if (TraceZapDeadLocals) warning("frame::zap_dead_entry_locals unimplemented");
}


void frame::zap_dead_deoptimized_locals(JavaThread*, const RegisterMap*) {
  if (TraceZapDeadLocals) warning("frame::zap_dead_deoptimized_locals unimplemented");
}

# endif // ENABLE_ZAP_DEAD_LOCALS

void frame::verify(const RegisterMap* map) {
  D_WARN_Unimplement;
}


#ifdef ASSERT
bool frame::verify_return_pc(address x) {
  if (StubRoutines::returns_to_call_stub(x)) {
    return true;
  }
  if (CodeCache::contains(x)) {
    return true;
  }
  if (Interpreter::contains(x)) {
    return true;
  }
  return false;
}
#endif

#ifdef ASSERT
void frame::interpreter_frame_verify_monitor(BasicObjectLock* value) const {
  assert(is_interpreted_frame(), "Not an interpreted frame");
  // verify that the value is in the right part of the frame
  address low_mark  = (address) interpreter_frame_monitor_end();
  address high_mark = (address) interpreter_frame_monitor_begin();
  address current   = (address) value;

  const int monitor_size = frame::interpreter_frame_monitor_size();
  guarantee((high_mark - current) % monitor_size  ==  0         , "Misaligned top of BasicObjectLock*");
  guarantee( high_mark > current                                , "Current BasicObjectLock* higher than high_mark");

  guarantee((current - low_mark) % monitor_size  ==  0         , "Misaligned bottom of BasicObjectLock*");
  guarantee( current >= low_mark                               , "Current BasicObjectLock* below than low_mark");
}
#endif

#ifndef PRODUCT
void frame::describe(FrameValues& values, int frame_no) {
D_WARN_Unimplement;
}

#endif


//-----------------------------------------------------------------------------------
// StackFrameStream implementation

StackFrameStream::StackFrameStream(JavaThread *thread, bool update) : _reg_map(thread, update) {
  assert(thread->has_last_Java_frame(), "sanity check");
  _fr = thread->last_frame();
  _is_done = false;
}


#ifndef PRODUCT

void FrameValues::describe(int owner, intptr_t* location, const char* description, int priority) {
  FrameValue fv;
  fv.location = location;
  fv.owner = owner;
  fv.priority = priority;
  fv.description = NEW_RESOURCE_ARRAY(char, strlen(description) + 1);
  strcpy(fv.description, description);
  _values.append(fv);
}


#ifdef ASSERT
void FrameValues::validate() {
  _values.sort(compare);
  bool error = false;
  FrameValue prev;
  prev.owner = -1;
  for (int i = _values.length() - 1; i >= 0; i--) {
    FrameValue fv = _values.at(i);
    if (fv.owner == -1) continue;
    if (prev.owner == -1) {
      prev = fv;
      continue;
    }
    if (prev.location == fv.location) {
      if (fv.owner != prev.owner) {
        tty->print_cr("overlapping storage");
        tty->print_cr(" " INTPTR_FORMAT ": " INTPTR_FORMAT " %s", prev.location, *prev.location, prev.description);
        tty->print_cr(" " INTPTR_FORMAT ": " INTPTR_FORMAT " %s", fv.location, *fv.location, fv.description);
        error = true;
      }
    } else {
      prev = fv;
    }
  }
  assert(!error, "invalid layout");
}
#endif // ASSERT

void FrameValues::print(JavaThread* thread) {
  _values.sort(compare);

  // Sometimes values like the fp can be invalid values if the
  // register map wasn't updated during the walk.  Trim out values
  // that aren't actually in the stack of the thread.
  int min_index = 0;
  int max_index = _values.length() - 1;
  intptr_t* v0 = _values.at(min_index).location;
  intptr_t* v1 = _values.at(max_index).location;

  if (thread == Thread::current()) {
    while (!thread->is_in_stack((address)v0)) {
      v0 = _values.at(++min_index).location;
    }
    while (!thread->is_in_stack((address)v1)) {
      v1 = _values.at(--max_index).location;
    }
  } else {
    while (!thread->on_local_stack((address)v0)) {
      v0 = _values.at(++min_index).location;
    }
    while (!thread->on_local_stack((address)v1)) {
      v1 = _values.at(--max_index).location;
    }
  }
  intptr_t* min = MIN2(v0, v1);
  intptr_t* max = MAX2(v0, v1);
  intptr_t* cur = max;
  intptr_t* last = NULL;
  for (int i = max_index; i >= min_index; i--) {
    FrameValue fv = _values.at(i);
    while (cur > fv.location) {
      tty->print_cr(" " INTPTR_FORMAT ": " INTPTR_FORMAT, cur, *cur);
      cur--;
    }
    if (last == fv.location) {
      const char* spacer = "          " LP64_ONLY("        ");
      tty->print_cr(" %s  %s %s", spacer, spacer, fv.description);
    } else {
      tty->print_cr(" " INTPTR_FORMAT ": " INTPTR_FORMAT " %s", fv.location, *fv.location, fv.description);
      last = fv.location;
      cur--;
    }
  }
}

#endif // ndef PRODUCT
