/*
 * Copyright (c) 1997, 2015, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/javaClasses.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/oopMapCache.hpp"
#include "memory/resourceArea.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/signature.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/synchronizer.hpp"
#include "runtime/vframe.hpp"

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

vframe::vframe(const frame* fr, const RegisterMap* reg_map, JavaThread* thread)
: _reg_map(reg_map), _thread(thread) {
  assert(fr != NULL, "must have frame");
  _fr = *fr;
}

vframe::vframe(const frame* fr, JavaThread* thread)
: _reg_map(thread), _thread(thread) {
  assert(fr != NULL, "must have frame");
  _fr = *fr;
}

vframe* vframe::new_vframe(const frame* f, const RegisterMap* reg_map, JavaThread* thread) {
  // Interpreter frame
  if (f->is_interpreted_frame()) {
    return new interpretedVFrame(f, reg_map, thread);
  }

  // External frame
  return new externalVFrame(f, reg_map, thread);
}

vframe* vframe::sender() const {
  RegisterMap temp_map = *register_map();
  assert(is_top(), "just checking");
  if (_fr.is_entry_frame() && _fr.is_first_frame()) return NULL;
  frame s = _fr.real_sender(&temp_map);
  if (s.is_first_frame()) return NULL;
  return vframe::new_vframe(&s, &temp_map, thread());
}

vframe* vframe::top() const {
  vframe* vf = (vframe*) this;
  while (!vf->is_top()) vf = vf->sender();
  return vf;
}


javaVFrame* vframe::java_sender() const {
  vframe* f = sender();
  while (f != NULL) {
    if (f->is_java_frame()) return javaVFrame::cast(f);
    f = f->sender();
  }
  return NULL;
}

// ------------- javaVFrame --------------

GrowableArray<MonitorInfo*>* javaVFrame::locked_monitors() {
  assert(SafepointSynchronize::is_at_safepoint() || JavaThread::current() == thread(),
         "must be at safepoint or it's a java frame of the current thread");

  D_WARN_Unimplement;
}

static void print_locked_object_class_name(outputStream* st, Handle obj, const char* lock_state) {
  if (obj.not_null()) {
    st->print("\t- %s <" INTPTR_FORMAT "> ", lock_state, (address)obj());
    if (obj->klass() == SystemDictionary::Class_klass()) {
      st->print_cr("(a java.lang.Class for %s)", java_lang_Class::as_external_name(obj()));
    } else {
      Klass* k = obj->klass();
      st->print_cr("(a %s)", k->external_name());
    }
  }
}

void javaVFrame::print_lock_info_on(outputStream* st, int frame_count) {
  ResourceMark rm;
  D_WARN_Unimplement;
}

// ------------- interpretedVFrame --------------

u_char* interpretedVFrame::bcp() const {
  return fr().interpreter_frame_bcp();
}

void interpretedVFrame::set_bcp(u_char* bcp) {
  fr().interpreter_frame_set_bcp(bcp);
}

intptr_t* interpretedVFrame::locals_addr_at(int offset) const {
  assert(fr().is_interpreted_frame(), "frame should be an interpreted frame");
  return fr().interpreter_frame_local_at(offset);
}


GrowableArray<MonitorInfo*>* interpretedVFrame::monitors() const {
  GrowableArray<MonitorInfo*>* result = new GrowableArray<MonitorInfo*>(5);
  for (BasicObjectLock* current = (fr().previous_monitor_in_interpreter_frame(fr().interpreter_frame_monitor_begin()));
       current >= fr().interpreter_frame_monitor_end();
       current = fr().previous_monitor_in_interpreter_frame(current)) {
    result->push(new MonitorInfo(current->obj(), current->lock(), false, false));
  }
  return result;
}

int interpretedVFrame::bci() const {
  return method()->bci_from(bcp());
}

Method* interpretedVFrame::method() const {
  return fr().interpreter_frame_method();
}

// ------------- cChunk --------------

entryVFrame::entryVFrame(const frame* fr, const RegisterMap* reg_map, JavaThread* thread)
: externalVFrame(fr, reg_map, thread) {}


void vframeStreamCommon::found_bad_method_frame() {
  // 6379830 Cut point for an assertion that occasionally fires when
  // we are using the performance analyzer.
  // Disable this assert when testing the analyzer with fastdebug.
  // -XX:SuppressErrorAt=vframe.cpp:XXX (XXX=following line number)
  assert(false, "invalid bci or invalid scope desc");
}

// top-frame will be skipped
vframeStream::vframeStream(JavaThread* thread, frame top_frame,
  bool stop_at_java_call_stub) : vframeStreamCommon(thread) {
  _stop_at_java_call_stub = stop_at_java_call_stub;

  // skip top frame, as it may not be at safepoint
  _frame  = top_frame.sender(&_reg_map);
  while (!fill_from_frame()) {
    _frame = _frame.sender(&_reg_map);
  }
}


// Step back n frames, skip any pseudo frames in between.
// This function is used in Class.forName, Class.newInstance, Method.Invoke,
// AccessController.doPrivileged.
void vframeStreamCommon::security_get_caller_frame(int depth) {
  assert(depth >= 0, err_msg("invalid depth: %d", depth));
  for (int n = 0; !at_end(); security_next()) {
    if (!method()->is_ignored_by_security_stack_walk()) {
      if (n == depth) {
        // We have reached the desired depth; return.
        return;
      }
      n++;  // this is a non-skipped frame; count it against the depth
    }
  }
  // NOTE: At this point there were not enough frames on the stack
  // to walk to depth.  Callers of this method have to check for at_end.
}


void vframeStreamCommon::security_next() {
  if (method()->is_prefixed_native()) {
    skip_prefixed_method_and_wrappers();  // calls next()
  } else {
    next();
  }
}


void vframeStreamCommon::skip_prefixed_method_and_wrappers() {
  ResourceMark rm;
  HandleMark hm;

  D_WARN_Unimplement;
}


void vframeStreamCommon::skip_reflection_related_frames() {
  while (!at_end() &&
         (JDK_Version::is_gte_jdk14x_version() && UseNewReflection &&
          (method()->method_holder()->is_subclass_of(SystemDictionary::reflect_MethodAccessorImpl_klass()) ||
           method()->method_holder()->is_subclass_of(SystemDictionary::reflect_ConstructorAccessorImpl_klass())))) {
    next();
  }
}


#ifndef PRODUCT
void vframe::print() {
  if (WizardMode) _fr.print_value_on(tty,NULL);
}


void vframe::print_value() const {
  ((vframe*)this)->print();
}


void entryVFrame::print_value() const {
  ((entryVFrame*)this)->print();
}

void entryVFrame::print() {
  vframe::print();
  tty->print_cr("C Chunk inbetween Java");
  tty->print_cr("C     link " INTPTR_FORMAT, _fr.link());
}


// ------------- javaVFrame --------------

static void print_stack_values(const char* title, StackValueCollection* values) {
  if (values->is_empty()) return;
  tty->print_cr("\t%s:", title);
  values->print();
}


void javaVFrame::print() {
  ResourceMark rm;
  vframe::print();
  tty->print("\t");
  method()->print_value();
  tty->cr();
  tty->print_cr("\tbci:    %d", bci());

  print_stack_values("locals",      locals());
  print_stack_values("expressions", expressions());

  GrowableArray<MonitorInfo*>* list = monitors();
  if (list->is_empty()) return;
  tty->print_cr("\tmonitor list:");
  for (int index = (list->length()-1); index >= 0; index--) {
    MonitorInfo* monitor = list->at(index);
    tty->print("\t  obj\t");
    if (monitor->owner_is_scalar_replaced()) {
      Klass* k = java_lang_Class::as_Klass(monitor->owner_klass());
      tty->print("( is scalar replaced %s)", k->external_name());
    } else if (monitor->owner() == NULL) {
      tty->print("( null )");
    } else {
      monitor->owner()->print_value();
      tty->print("(owner=" INTPTR_FORMAT ")", (address)monitor->owner());
    }
    if (monitor->eliminated()) {
      if(is_compiled_frame()) {
        tty->print(" ( lock is eliminated in compiled frame )");
      } else {
        tty->print(" ( lock is eliminated, frame not compiled )");
      }
    }
    tty->cr();
    tty->print("\t  ");
    monitor->lock()->print_on(tty);
    tty->cr();
  }
}


void javaVFrame::print_value() const {
  Method*    m = method();
  InstanceKlass*     k = m->method_holder();
  tty->print_cr("frame( sp=" INTPTR_FORMAT ", unextended_sp=" INTPTR_FORMAT ", fp=" INTPTR_FORMAT ", pc=" INTPTR_FORMAT ")",
                _fr.sp(),  _fr.unextended_sp(), _fr.fp(), _fr.pc());
  tty->print("%s.%s", k->internal_name(), m->name()->as_C_string());

  if (!m->is_native()) {
    Symbol*  source_name = k->source_file_name();
    int        line_number = m->line_number_from_bci(bci());
    if (source_name != NULL && (line_number != -1)) {
      tty->print("(%s:%d)", source_name->as_C_string(), line_number);
    }
  } else {
    tty->print("(Native Method)");
  }
  // Check frame size and print warning if it looks suspiciously large
  if (fr().sp() != NULL) {
    RegisterMap map = *register_map();
    uint size = fr().frame_size(&map);
#ifdef _LP64
    if (size > 8*K) warning("SUSPICIOUSLY LARGE FRAME (%d)", size);
#else
    if (size > 4*K) warning("SUSPICIOUSLY LARGE FRAME (%d)", size);
#endif
  }
}


bool javaVFrame::structural_compare(javaVFrame* other) {
  // Check static part
  if (method() != other->method()) return false;
  if (bci()    != other->bci())    return false;

  // Check locals
  StackValueCollection *locs = locals();
  StackValueCollection *other_locs = other->locals();
  assert(locs->size() == other_locs->size(), "sanity check");
  int i;
  for(i = 0; i < locs->size(); i++) {
    // it might happen the compiler reports a conflict and
    // the interpreter reports a bogus int.
    if (       is_compiled_frame() &&       locs->at(i)->type() == T_CONFLICT) continue;
    if (other->is_compiled_frame() && other_locs->at(i)->type() == T_CONFLICT) continue;

    if (!locs->at(i)->equal(other_locs->at(i)))
      return false;
  }

  // Check expressions
  StackValueCollection* exprs = expressions();
  StackValueCollection* other_exprs = other->expressions();
  assert(exprs->size() == other_exprs->size(), "sanity check");
  for(i = 0; i < exprs->size(); i++) {
    if (!exprs->at(i)->equal(other_exprs->at(i)))
      return false;
  }

  return true;
}


void javaVFrame::print_activation(int index) const {
  // frame number and method
  tty->print("%2d - ", index);
  ((vframe*)this)->print_value();
  tty->cr();

  if (WizardMode) {
    ((vframe*)this)->print();
    tty->cr();
  }
}


void javaVFrame::verify() const {
}


void interpretedVFrame::verify() const {
}


// ------------- externalVFrame --------------

void externalVFrame::print() {
  _fr.print_value_on(tty,NULL);
}


void externalVFrame::print_value() const {
  ((vframe*)this)->print();
}
#endif // PRODUCT
