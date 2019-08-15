/*
 * Copyright (c) 1997, 2016, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/vmSymbols.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "interpreter/templateTable.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.inline.hpp"

// vm_version is removed
//# include "vm_version_x86.hpp"

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

#define IRT_ENTRY(ret, header) ret header { \
    Thread* THREAD = (Thread*)thread;
#define IRT_END }

#include "enclave/sc/EnclaveException.h"

IRT_ENTRY(oop, InterpreterRuntime::create_exception(JavaThread* thread, char* name, char* message))
  // lookup exception klass
  // create exception
  return (instanceOop)EnclaveException::get_exception_oop(thread, name);
IRT_END

IRT_ENTRY(oop, InterpreterRuntime::create_klass_exception(JavaThread* thread, char* name, oopDesc* obj))
  // create exception, with klass name as detail message
  return (instanceOop)EnclaveException::get_exception_oop(thread, name);
IRT_END

IRT_ENTRY(void, InterpreterRuntime::throw_ClassCastException(JavaThread* thread, oopDesc* obj))
  ENCLAVE_THROW(EnclaveException::java_lang_ClassCastException);
IRT_END

IRT_ENTRY(void, InterpreterRuntime::throw_ArrayIndexOutOfBoundsException(JavaThread* thread, char* name, jint index))
  ENCLAVE_THROW(EnclaveException::java_lang_ArrayIndexOutOfBoundsException);
IRT_END

IRT_ENTRY(void, InterpreterRuntime::throw_StackOverflowError(JavaThread* thread))
  ENCLAVE_THROW(EnclaveException::java_lang_StackOverflowError);
IRT_END

IRT_ENTRY(void, InterpreterRuntime::throw_AbstractMethodError(JavaThread* thread))
  ENCLAVE_THROW(EnclaveException::java_lang_AbstractMethodError);
IRT_END

IRT_ENTRY(address, InterpreterRuntime::exception_handler_for_exception(JavaThread* thread, oopDesc* exception))
    BREAKPOINT;
  Handle             h_exception(thread, exception);
  methodHandle       h_method   (thread, method(thread));
  constantPoolHandle h_constants(thread, h_method->constants());
  bool               should_repeat;
  int                handler_bci;
  int                current_bci = bci(thread);

  do {
    should_repeat = false;

// Don't go paging in something which won't be used.
//     else if (extable->length() == 0) {
//       // disabled for now - interpreter is not using shortcut yet
//       // (shortcut is not to call runtime if we have no exception handlers)
//       // warning("performance bug: should not call runtime if method has no exception handlers");
//     }

    // exception handler lookup
    KlassHandle h_klass(THREAD, h_exception->klass());
    handler_bci = Method::fast_exception_handler_bci_for(h_method, h_klass, current_bci, THREAD);
    if (HAS_PENDING_EXCEPTION) {
      // We threw an exception while trying to find the exception handler.
      // Transfer the new exception to the exception handle which will
      // be set into thread local storage, and do another lookup for an
      // exception handler for this exception, this time starting at the
      // BCI of the exception handler which caused the exception to be
      // thrown (bug 4307310).
      h_exception = Handle(THREAD, PENDING_EXCEPTION);
      CLEAR_PENDING_EXCEPTION;
      if (handler_bci >= 0) {
        current_bci = handler_bci;
        should_repeat = true;
      }
    }
  } while (should_repeat == true);

  address continuation = NULL;
  address handler_pc = NULL;
  if (handler_bci < 0) {
    // Forward exception to callee (leaving bci/bcp untouched) because (a) no
    // handler in this method, or (b) after a stack overflow there is not yet
    // enough stack space available to reprotect the stack.
    continuation = Interpreter::remove_activation_entry();
    // Count this for compilation purposes
    h_method->interpreter_throwout_increment(THREAD);
  } else {
    // handler in this method => change bci/bcp to handler bci/bcp and continue there
    handler_pc = h_method->code_base() + handler_bci;
    set_bcp_and_mdp(handler_pc, thread);
    continuation = Interpreter::dispatch_table(vtos)[*handler_pc];
  }
  // notify debugger of an exception catch
  // (this is good for exceptions caught in native methods as well)

  thread->set_vm_result(h_exception());
  return continuation;
IRT_END

// Implementation of SignatureHandlerLibrary

address SignatureHandlerLibrary::set_handler_blob() {
  BufferBlob* handler_blob = BufferBlob::create("native signature handlers", blob_size);
  if (handler_blob == NULL) {
    return NULL;
  }
  address handler = handler_blob->code_begin();
  _handler_blob = handler_blob;
  _handler = handler;
  return handler;
}

void SignatureHandlerLibrary::initialize() {
  if (_fingerprints != NULL) {
    return;
  }
  if (set_handler_blob() == NULL) {
    // TODO: throw out_of_memory
  }

  BufferBlob* bb = BufferBlob::create("Signature Handler Temp Buffer",
                                      SignatureHandlerLibrary::buffer_size);
  _buffer = bb->code_begin();

  _fingerprints = new(ResourceObj::C_HEAP, mtCode)GrowableArray<uint64_t>(32, true);
  _handlers     = new(ResourceObj::C_HEAP, mtCode)GrowableArray<address>(32, true);
}

address SignatureHandlerLibrary::set_handler(CodeBuffer* buffer) {
  address handler   = _handler;
  int     insts_size = buffer->pure_insts_size();
  if (handler + insts_size > _handler_blob->code_end()) {
    // get a new handler blob
    handler = set_handler_blob();
  }
  if (handler != NULL) {
    memcpy(handler, buffer->insts_begin(), insts_size);
    pd_set_handler(handler);
    ICache::invalidate_range(handler, insts_size);
    _handler = handler + insts_size;
  }
  return handler;
}

void SignatureHandlerLibrary::add(methodHandle method) {
  if (method->enclave_signature_handler() == NULL) {
    // use slow signature handler if we can't do better
    int handler_index = -1;
    // check if we can use customized (fast) signature handler
    if (UseFastSignatureHandlers && method->size_of_parameters() <= Fingerprinter::max_size_of_parameters) {
      // use customized signature handler

      // make sure data structure is initialized
      initialize();
      // lookup method signature's fingerprint
      uint64_t fingerprint = Fingerprinter(method).fingerprint();
      handler_index = _fingerprints->find(fingerprint);
      // create handler if necessary
      if (handler_index < 0) {
        ptrdiff_t align_offset = (address)
          round_to((intptr_t)_buffer, CodeEntryAlignment) - (address)_buffer;
        CodeBuffer buffer((address)(_buffer + align_offset),
                          SignatureHandlerLibrary::buffer_size - align_offset);
        InterpreterRuntime::SignatureHandlerGenerator(method, &buffer).generate(fingerprint);
        // copy into code heap
        address handler = set_handler(&buffer);
        if (handler == NULL) {
          // use slow signature handler
        } else {
          // add handler to library
          _fingerprints->append(fingerprint);
          _handlers->append(handler);
          // set handler index
          assert(_fingerprints->length() == _handlers->length(), "sanity check");
          handler_index = _fingerprints->length() - 1;
        }
      }
      // Set handler under SignatureHandlerLibrary_lock
    if (handler_index < 0) {
      // use generic signature handler
      method->set_enclave_signature_handler(Interpreter::slow_signature_handler());
    } else {
      // set handler
      method->set_enclave_signature_handler(_handlers->at(handler_index));
    }
    } else {
      CHECK_UNHANDLED_OOPS_ONLY(Thread::current()->clear_unhandled_oops());
      // use generic signature handler
      method->set_signature_handler(Interpreter::slow_signature_handler());
    }
  }
}


BufferBlob*              SignatureHandlerLibrary::_handler_blob = NULL;
address                  SignatureHandlerLibrary::_handler      = NULL;
GrowableArray<uint64_t>* SignatureHandlerLibrary::_fingerprints = NULL;
GrowableArray<address>*  SignatureHandlerLibrary::_handlers     = NULL;
address                  SignatureHandlerLibrary::_buffer       = NULL;
