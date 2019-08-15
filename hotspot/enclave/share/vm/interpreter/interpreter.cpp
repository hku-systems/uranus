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
#include "asm/macroAssembler.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "interpreter/templateTable.hpp"
#include "memory/allocation.inline.hpp"
//#include "oops/arrayOop.hpp"
//#include "oops/methodData.hpp"
#include "oops/method.hpp"
#include "oops/oop.inline.hpp"
//#include "prims/forte.hpp"
//#include "prims/methodHandles.hpp"
#include "runtime/handles.inline.hpp"

# define __ _masm->


//------------------------------------------------------------------------------------------------------------------------
// Implementation of InterpreterCodelet

void InterpreterCodelet::initialize(const char* description, Bytecodes::Code bytecode) {
  _description       = description;
  _bytecode          = bytecode;
}


void InterpreterCodelet::verify() {
}


void InterpreterCodelet::print_on(outputStream* st) const {
  //
}


//------------------------------------------------------------------------------------------------------------------------
// Implementation of  platform independent aspects of Interpreter

void AbstractInterpreter::initialize() {
  if (_code != NULL) return;
}

void AbstractInterpreter::print() {

}


void interpreter_init() {
  Interpreter::initialize();
  // forte is ignored
}

//------------------------------------------------------------------------------------------------------------------------
// Implementation of interpreter

StubQueue* AbstractInterpreter::_code                                       = NULL;
bool       AbstractInterpreter::_notice_safepoints                          = false;
address    AbstractInterpreter::_rethrow_exception_entry                    = NULL;

address    AbstractInterpreter::_native_entry_begin                         = NULL;
address    AbstractInterpreter::_native_entry_end                           = NULL;
address    AbstractInterpreter::_slow_signature_handler;
address    AbstractInterpreter::_entry_table            [AbstractInterpreter::number_of_method_entries];
address    AbstractInterpreter::_native_abi_to_tosca    [AbstractInterpreter::number_of_result_handlers];

//------------------------------------------------------------------------------------------------------------------------
// Generation of complete interpreter

AbstractInterpreterGenerator::AbstractInterpreterGenerator(StubQueue* _code) {
  _masm                      = NULL;
}


static const BasicType types[Interpreter::number_of_result_handlers] = {
  T_BOOLEAN,
  T_CHAR   ,
  T_BYTE   ,
  T_SHORT  ,
  T_INT    ,
  T_LONG   ,
  T_VOID   ,
  T_FLOAT  ,
  T_DOUBLE ,
  T_OBJECT
};

void AbstractInterpreterGenerator::generate_all() {


  {
    CodeletMark cm(_masm, "slow signature handler");
    Interpreter::_slow_signature_handler = generate_slow_signature_handler();
  }

}

//------------------------------------------------------------------------------------------------------------------------
// Entry points

AbstractInterpreter::MethodKind AbstractInterpreter::method_kind(methodHandle m) {
  // Abstract method?
  if (m->is_abstract()) return abstract;

  // Method handle primitive?
  if (m->is_method_handle_intrinsic()) {
    vmIntrinsics::ID id = m->intrinsic_id();
    assert(MethodHandles::is_signature_polymorphic(id), "must match an intrinsic");
    MethodKind kind = (MethodKind)( method_handle_invoke_FIRST +
                                    ((int)id - vmIntrinsics::FIRST_MH_SIG_POLY) );
    assert(kind <= method_handle_invoke_LAST, "parallel enum ranges");
    return kind;
  }

#ifndef CC_INTERP
  if (UseCRC32Intrinsics && m->is_native()) {
    // Use optimized stub code for CRC32 native methods.
    switch (m->intrinsic_id()) {
      case vmIntrinsics::_updateCRC32            : return java_util_zip_CRC32_update;
      case vmIntrinsics::_updateBytesCRC32       : return java_util_zip_CRC32_updateBytes;
      case vmIntrinsics::_updateByteBufferCRC32  : return java_util_zip_CRC32_updateByteBuffer;
    }
  }
#endif

  // Native method?
  // Note: This test must come _before_ the test for intrinsic
  //       methods. See also comments below.
  if (m->is_native()) {
    assert(!m->is_method_handle_intrinsic(), "overlapping bits here, watch out");
    return m->is_synchronized() ? native_synchronized : native;
  }

  // Synchronized?
  if (m->is_synchronized()) {
    return zerolocals_synchronized;
  }

  if (RegisterFinalizersAtInit && m->code_size() == 1 &&
      m->intrinsic_id() == vmIntrinsics::_Object_init) {
    // We need to execute the special return bytecode to check for
    // finalizer registration so create a normal frame.
    return zerolocals;
  }

  // Empty method?
  if (m->is_empty_method()) {
    return empty;
  }

  // Special intrinsic method?
  // Note: This test must come _after_ the test for native methods,
  //       otherwise we will run into problems with JDK 1.2, see also
  //       AbstractInterpreterGenerator::generate_method_entry() for
  //       for details.
  switch (m->intrinsic_id()) {
    case vmIntrinsics::_dsin  : return java_lang_math_sin  ;
    case vmIntrinsics::_dcos  : return java_lang_math_cos  ;
    case vmIntrinsics::_dtan  : return java_lang_math_tan  ;
    case vmIntrinsics::_dabs  : return java_lang_math_abs  ;
    case vmIntrinsics::_dsqrt : return java_lang_math_sqrt ;
    case vmIntrinsics::_dlog  : return java_lang_math_log  ;
    case vmIntrinsics::_dlog10: return java_lang_math_log10;
    case vmIntrinsics::_dpow  : return java_lang_math_pow  ;
    case vmIntrinsics::_dexp  : return java_lang_math_exp  ;

    case vmIntrinsics::_Reference_get:
                                return java_lang_ref_reference_get;
  }

  // Accessor method?
  if (m->is_accessor()) {
    assert(m->size_of_parameters() == 1, "fast code for accessors assumes parameter size = 1");
    return accessor;
  }

  // Note: for now: zero locals for all non-empty methods
  return zerolocals;
}


void AbstractInterpreter::set_entry_for_kind(AbstractInterpreter::MethodKind kind, address entry) {
  assert(kind >= method_handle_invoke_FIRST &&
         kind <= method_handle_invoke_LAST, "late initialization only for MH entry points");
  assert(_entry_table[kind] == _entry_table[abstract], "previous value must be AME entry");
  _entry_table[kind] = entry;
}


// Return true if the interpreter can prove that the given bytecode has
// not yet been executed (in Java semantics, not in actual operation).
bool AbstractInterpreter::is_not_reached(methodHandle method, int bci) {
  Bytecodes::Code code = method()->code_at(bci);

  if (!Bytecodes::must_rewrite(code)) {
    // might have been reached
    return false;
  }

  // the bytecode might not be rewritten if the method is an accessor, etc.
  address ientry = method->interpreter_entry();
  if (ientry != entry_for_kind(AbstractInterpreter::zerolocals) &&
      ientry != entry_for_kind(AbstractInterpreter::zerolocals_synchronized))
    return false;  // interpreter does not run this method!

  // otherwise, we can be sure this bytecode has never been executed
  return true;
}


#ifndef PRODUCT
void AbstractInterpreter::print_method_kind(MethodKind kind) {
}
#endif // PRODUCT


//------------------------------------------------------------------------------------------------------------------------
// Deoptimization support

/**
 * If a deoptimization happens, this function returns the point of next bytecode to continue execution.
 */
// deleted

void AbstractInterpreterGenerator::bang_stack_shadow_pages(bool native_call) {
  // Quick & dirty stack overflow checking: bang the stack & handle trap.
  // Note that we do the banging after the frame is setup, since the exception
  // handling code expects to find a valid interpreter frame on the stack.
  // Doing the banging earlier fails if the caller frame is not an interpreter
  // frame.
  // (Also, the exception throwing code expects to unlock any synchronized
  // method receiever, so do the banging after locking the receiver.)

  // Bang each page in the shadow zone. We can't assume it's been done for
  // an interpreter frame with greater than a page of locals, so each page
  // needs to be checked.  Only true for non-native.
  if (UseStackBanging) {
    // TODO: magic number for StackOverflowException

    __ push(r10);
    __ movptr(r10, 0xfe3f912);

    const int start_page = native_call ? StackShadowPages : 1;
    const int page_size = os::vm_page_size();
    for (int pages = start_page; pages <= StackShadowPages ; pages++) {
      __ bang_stack_with_offset(pages*page_size);
    }

    __ pop(r10);
  }
}

void AbstractInterpreterGenerator::initialize_method_handle_entries() {
  // method handle entry kinds are generated later in MethodHandlesAdapterGenerator::generate:
  for (int i = Interpreter::method_handle_invoke_FIRST; i <= Interpreter::method_handle_invoke_LAST; i++) {
    Interpreter::MethodKind kind = (Interpreter::MethodKind) i;
    Interpreter::_entry_table[kind] = Interpreter::_entry_table[Interpreter::abstract];
  }
}
