/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_RUNTIME_SHAREDRUNTIME_HPP
#define SHARE_VM_RUNTIME_SHAREDRUNTIME_HPP

#include "memory/allocation.hpp"
#include "memory/resourceArea.hpp"
#include "utilities/hashtable.hpp"
#include "utilities/macros.hpp"

class AdapterHandlerEntry;
class AdapterHandlerTable;
class AdapterFingerPrint;
class vframeStream;

// Runtime is the base class for various runtime interfaces
// (InterpreterRuntime, CompilerRuntime, etc.). It provides
// shared functionality such as exception forwarding (C++ to
// Java exceptions), locking/unlocking mechanisms, statistical
// information, etc.

class SharedRuntime: AllStatic {
  friend class VMStructs;

 private:
  static methodHandle resolve_sub_helper(JavaThread *thread,
                                     bool is_virtual,
                                     bool is_optimized, TRAPS);

  // Shared stub locations

 private:
  enum { POLL_AT_RETURN,  POLL_AT_LOOP, POLL_AT_VECTOR_LOOP };

 public:
  static void generate_stubs(void);

  // max bytes for each dtrace string parameter
  enum { max_dtrace_string_size = 256 };

  // The following arithmetic routines are used on platforms that do
  // not have machine instructions to implement their functionality.
  // Do not remove these.

  // long arithmetics
  static jlong   lmul(jlong y, jlong x);
  static jlong   ldiv(jlong y, jlong x);
  static jlong   lrem(jlong y, jlong x);

  // float and double remainder
  static jfloat  frem(jfloat  x, jfloat  y);
  static jdouble drem(jdouble x, jdouble y);

#ifdef __SOFTFP__
  static jfloat  fadd(jfloat x, jfloat y);
  static jfloat  fsub(jfloat x, jfloat y);
  static jfloat  fmul(jfloat x, jfloat y);
  static jfloat  fdiv(jfloat x, jfloat y);

  static jdouble dadd(jdouble x, jdouble y);
  static jdouble dsub(jdouble x, jdouble y);
  static jdouble dmul(jdouble x, jdouble y);
  static jdouble ddiv(jdouble x, jdouble y);
#endif // __SOFTFP__

  // float conversion (needs to set appropriate rounding mode)
  static jint    f2i (jfloat  x);
  static jlong   f2l (jfloat  x);
  static jint    d2i (jdouble x);
  static jlong   d2l (jdouble x);
  static jfloat  d2f (jdouble x);
  static jfloat  l2f (jlong   x);
  static jdouble l2d (jlong   x);

#ifdef __SOFTFP__
  static jfloat  i2f (jint    x);
  static jdouble i2d (jint    x);
  static jdouble f2d (jfloat  x);
#endif // __SOFTFP__

  // double trigonometrics and transcendentals
  static jdouble dsin(jdouble x);
  static jdouble dcos(jdouble x);
  static jdouble dtan(jdouble x);
  static jdouble dlog(jdouble x);
  static jdouble dlog10(jdouble x);
  static jdouble dexp(jdouble x);
  static jdouble dpow(jdouble x, jdouble y);

#if defined(__SOFTFP__) || defined(E500V2)
  static double dabs(double f);
#endif

#if defined(__SOFTFP__) || defined(PPC32)
  static double dsqrt(double f);
#endif

  // Montgomery multiplication
  static void montgomery_multiply(jint *a_ints, jint *b_ints, jint *n_ints,
                                  jint len, jlong inv, jint *m_ints);
  static void montgomery_square(jint *a_ints, jint *n_ints,
                                jint len, jlong inv, jint *m_ints);

#ifdef __SOFTFP__
  // C++ compiler generates soft float instructions as well as passing
  // float and double in registers.
  static int  fcmpl(float x, float y);
  static int  fcmpg(float x, float y);
  static int  dcmpl(double x, double y);
  static int  dcmpg(double x, double y);

  static int unordered_fcmplt(float x, float y);
  static int unordered_dcmplt(double x, double y);
  static int unordered_fcmple(float x, float y);
  static int unordered_dcmple(double x, double y);
  static int unordered_fcmpge(float x, float y);
  static int unordered_dcmpge(double x, double y);
  static int unordered_fcmpgt(float x, float y);
  static int unordered_dcmpgt(double x, double y);

  static float  fneg(float f);
  static double dneg(double f);
#endif

  // To be used as the entry point for unresolved native methods.
  static address native_method_throw_unsatisfied_link_error_entry();

};


// ---------------------------------------------------------------------------
// Implementation of AdapterHandlerLibrary
//
// This library manages argument marshaling adapters and native wrappers.
// There are 2 flavors of adapters: I2C and C2I.
//
// The I2C flavor takes a stock interpreted call setup, marshals the
// arguments for a Java-compiled call, and jumps to Rmethod-> code()->
// code_begin().  It is broken to call it without an nmethod assigned.
// The usual behavior is to lift any register arguments up out of the
// stack and possibly re-pack the extra arguments to be contigious.
// I2C adapters will save what the interpreter's stack pointer will be
// after arguments are popped, then adjust the interpreter's frame
// size to force alignment and possibly to repack the arguments.
// After re-packing, it jumps to the compiled code start.  There are
// no safepoints in this adapter code and a GC cannot happen while
// marshaling is in progress.
//
// The C2I flavor takes a stock compiled call setup plus the target method in
// Rmethod, marshals the arguments for an interpreted call and jumps to
// Rmethod->_i2i_entry.  On entry, the interpreted frame has not yet been
// setup.  Compiled frames are fixed-size and the args are likely not in the
// right place.  Hence all the args will likely be copied into the
// interpreter's frame, forcing that frame to grow.  The compiled frame's
// outgoing stack args will be dead after the copy.
//
// Native wrappers, like adapters, marshal arguments.  Unlike adapters they
// also perform an offical frame push & pop.  They have a call to the native
// routine in their middles and end in a return (instead of ending in a jump).
// The native wrappers are stored in real nmethods instead of the BufferBlobs
// used by the adapters.  The code generation happens here because it's very
// similar to what the adapters have to do.

class AdapterHandlerEntry : public BasicHashtableEntry<mtCode> {
  friend class AdapterHandlerTable;

 private:
  AdapterFingerPrint* _fingerprint;
  address _i2c_entry;
  address _c2i_entry;
  address _c2i_unverified_entry;

#ifdef ASSERT
  // Captures code and signature used to generate this adapter when
  // verifing adapter equivalence.
  unsigned char* _saved_code;
  int            _saved_code_length;
#endif

  void init(AdapterFingerPrint* fingerprint, address i2c_entry, address c2i_entry, address c2i_unverified_entry) {
    _fingerprint = fingerprint;
    _i2c_entry = i2c_entry;
    _c2i_entry = c2i_entry;
    _c2i_unverified_entry = c2i_unverified_entry;
#ifdef ASSERT
    _saved_code = NULL;
    _saved_code_length = 0;
#endif
  }

  void deallocate();

  // should never be used
  AdapterHandlerEntry();

 public:
  address get_i2c_entry()            const { return _i2c_entry; }
  address get_c2i_entry()            const { return _c2i_entry; }
  address get_c2i_unverified_entry() const { return _c2i_unverified_entry; }
  address base_address();
  void relocate(address new_base);

  AdapterFingerPrint* fingerprint() const { return _fingerprint; }

  AdapterHandlerEntry* next() {
    return (AdapterHandlerEntry*)BasicHashtableEntry<mtCode>::next();
  }

#ifdef ASSERT
  // Used to verify that code generated for shared adapters is equivalent
  void save_code   (unsigned char* code, int length);
  bool compare_code(unsigned char* code, int length);
#endif

  //virtual void print_on(outputStream* st) const;  DO NOT USE
  void print_adapter_on(outputStream* st) const;
};

class AdapterHandlerLibrary: public AllStatic {
 private:
  static BufferBlob* _buffer; // the temporary code buffer in CodeCache
  static AdapterHandlerTable* _adapters;
  static AdapterHandlerEntry* _abstract_method_handler;
  static BufferBlob* buffer_blob();
  static void initialize();

 public:

  static AdapterHandlerEntry* new_entry(AdapterFingerPrint* fingerprint,
                                        address i2c_entry, address c2i_entry, address c2i_unverified_entry);
  static void create_native_wrapper(methodHandle method);
  static AdapterHandlerEntry* get_adapter(methodHandle method);

#ifdef HAVE_DTRACE_H
  static nmethod* create_dtrace_nmethod (methodHandle method);
#endif // HAVE_DTRACE_H

  static void print_handler(CodeBlob* b) { print_handler_on(tty, b); }
  static void print_handler_on(outputStream* st, CodeBlob* b);
  static bool contains(CodeBlob* b);
#ifndef PRODUCT
  static void print_statistics();
#endif /* PRODUCT */

};

#endif // SHARE_VM_RUNTIME_SHAREDRUNTIME_HPP
