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
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "memory/universe.inline.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "utilities/copy.hpp"
#include "utilities/hashtable.inline.hpp"
#include "utilities/macros.hpp"
#ifdef TARGET_ARCH_x86
# include "nativeInst_x86.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "nativeInst_sparc.hpp"
# include "vmreg_sparc.inline.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "nativeInst_zero.hpp"
# include "vmreg_zero.inline.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "nativeInst_arm.hpp"
# include "vmreg_arm.inline.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "nativeInst_ppc.hpp"
# include "vmreg_ppc.inline.hpp"
#endif
#ifdef COMPILER1
#include "c1/c1_Runtime1.hpp"
#endif

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

#define JRT_BLOCK_ENTRY(result_type, header) result_type header {
#define IRT_LEAF(result_type, header) result_type header {
#define JRT_ENTRY_NO_ASYNC(result_type, header) \
  result_type header {  \
  Thread* THREAD = thread;
#define JNI_ENTRY(result_type, header) result_type header {
#define JRT_ENTRY(result_type, header) \
  result_type header {  \
  Thread* THREAD = thread;
#define JRT_LEAF(result_type, header) result_type header {
#define JRT_END }
#define JNI_END }
#define IRT_END }
#define JRT_BLOCK {
#define JRT_BLOCK_END }

//----------------------------generate_stubs-----------------------------------
void SharedRuntime::generate_stubs() {

}

#include <math.h>


// Implementation of SharedRuntime

JRT_LEAF(jlong, SharedRuntime::lmul(jlong y, jlong x))
  return x * y;
JRT_END


JRT_LEAF(jlong, SharedRuntime::ldiv(jlong y, jlong x))
  if (x == min_jlong && y == CONST64(-1)) {
    return x;
  } else {
    return x / y;
  }
JRT_END


JRT_LEAF(jlong, SharedRuntime::lrem(jlong y, jlong x))
  if (x == min_jlong && y == CONST64(-1)) {
    return 0;
  } else {
    return x % y;
  }
JRT_END


const juint  float_sign_mask  = 0x7FFFFFFF;
const juint  float_infinity   = 0x7F800000;
const julong double_sign_mask = CONST64(0x7FFFFFFFFFFFFFFF);
const julong double_infinity  = CONST64(0x7FF0000000000000);

JRT_LEAF(jfloat, SharedRuntime::frem(jfloat  x, jfloat  y))
#ifdef _WIN64
  // 64-bit Windows on amd64 returns the wrong values for
  // infinity operands.
  union { jfloat f; juint i; } xbits, ybits;
  xbits.f = x;
  ybits.f = y;
  // x Mod Infinity == x unless x is infinity
  if ( ((xbits.i & float_sign_mask) != float_infinity) &&
       ((ybits.i & float_sign_mask) == float_infinity) ) {
    return x;
  }
#endif
  return ((jfloat)fmod((double)x,(double)y));
JRT_END


JRT_LEAF(jdouble, SharedRuntime::drem(jdouble x, jdouble y))
#ifdef _WIN64
  union { jdouble d; julong l; } xbits, ybits;
  xbits.d = x;
  ybits.d = y;
  // x Mod Infinity == x unless x is infinity
  if ( ((xbits.l & double_sign_mask) != double_infinity) &&
       ((ybits.l & double_sign_mask) == double_infinity) ) {
    return x;
  }
#endif
  return ((jdouble)fmod((double)x,(double)y));
JRT_END

#ifdef __SOFTFP__
JRT_LEAF(jfloat, SharedRuntime::fadd(jfloat x, jfloat y))
  return x + y;
JRT_END

JRT_LEAF(jfloat, SharedRuntime::fsub(jfloat x, jfloat y))
  return x - y;
JRT_END

JRT_LEAF(jfloat, SharedRuntime::fmul(jfloat x, jfloat y))
  return x * y;
JRT_END

JRT_LEAF(jfloat, SharedRuntime::fdiv(jfloat x, jfloat y))
  return x / y;
JRT_END

JRT_LEAF(jdouble, SharedRuntime::dadd(jdouble x, jdouble y))
  return x + y;
JRT_END

JRT_LEAF(jdouble, SharedRuntime::dsub(jdouble x, jdouble y))
  return x - y;
JRT_END

JRT_LEAF(jdouble, SharedRuntime::dmul(jdouble x, jdouble y))
  return x * y;
JRT_END

JRT_LEAF(jdouble, SharedRuntime::ddiv(jdouble x, jdouble y))
  return x / y;
JRT_END

JRT_LEAF(jfloat, SharedRuntime::i2f(jint x))
  return (jfloat)x;
JRT_END

JRT_LEAF(jdouble, SharedRuntime::i2d(jint x))
  return (jdouble)x;
JRT_END

JRT_LEAF(jdouble, SharedRuntime::f2d(jfloat x))
  return (jdouble)x;
JRT_END

JRT_LEAF(int,  SharedRuntime::fcmpl(float x, float y))
  return x>y ? 1 : (x==y ? 0 : -1);  /* x<y or is_nan*/
JRT_END

JRT_LEAF(int,  SharedRuntime::fcmpg(float x, float y))
  return x<y ? -1 : (x==y ? 0 : 1);  /* x>y or is_nan */
JRT_END

JRT_LEAF(int,  SharedRuntime::dcmpl(double x, double y))
  return x>y ? 1 : (x==y ? 0 : -1); /* x<y or is_nan */
JRT_END

JRT_LEAF(int,  SharedRuntime::dcmpg(double x, double y))
  return x<y ? -1 : (x==y ? 0 : 1);  /* x>y or is_nan */
JRT_END

// Functions to return the opposite of the aeabi functions for nan.
JRT_LEAF(int, SharedRuntime::unordered_fcmplt(float x, float y))
  return (x < y) ? 1 : ((g_isnan(x) || g_isnan(y)) ? 1 : 0);
JRT_END

JRT_LEAF(int, SharedRuntime::unordered_dcmplt(double x, double y))
  return (x < y) ? 1 : ((g_isnan(x) || g_isnan(y)) ? 1 : 0);
JRT_END

JRT_LEAF(int, SharedRuntime::unordered_fcmple(float x, float y))
  return (x <= y) ? 1 : ((g_isnan(x) || g_isnan(y)) ? 1 : 0);
JRT_END

JRT_LEAF(int, SharedRuntime::unordered_dcmple(double x, double y))
  return (x <= y) ? 1 : ((g_isnan(x) || g_isnan(y)) ? 1 : 0);
JRT_END

JRT_LEAF(int, SharedRuntime::unordered_fcmpge(float x, float y))
  return (x >= y) ? 1 : ((g_isnan(x) || g_isnan(y)) ? 1 : 0);
JRT_END

JRT_LEAF(int, SharedRuntime::unordered_dcmpge(double x, double y))
  return (x >= y) ? 1 : ((g_isnan(x) || g_isnan(y)) ? 1 : 0);
JRT_END

JRT_LEAF(int, SharedRuntime::unordered_fcmpgt(float x, float y))
  return (x > y) ? 1 : ((g_isnan(x) || g_isnan(y)) ? 1 : 0);
JRT_END

JRT_LEAF(int, SharedRuntime::unordered_dcmpgt(double x, double y))
  return (x > y) ? 1 : ((g_isnan(x) || g_isnan(y)) ? 1 : 0);
JRT_END

// Intrinsics make gcc generate code for these.
float  SharedRuntime::fneg(float f)   {
  return -f;
}

double SharedRuntime::dneg(double f)  {
  return -f;
}

#endif // __SOFTFP__

#if defined(__SOFTFP__) || defined(E500V2)
// Intrinsics make gcc generate code for these.
double SharedRuntime::dabs(double f)  {
  return (f <= (double)0.0) ? (double)0.0 - f : f;
}

#endif

#if defined(__SOFTFP__) || defined(PPC32)
double SharedRuntime::dsqrt(double f) {
  return sqrt(f);
}
#endif

JRT_LEAF(jint, SharedRuntime::f2i(jfloat  x))
  if (g_isnan(x))
    return 0;
  if (x >= (jfloat) max_jint)
    return max_jint;
  if (x <= (jfloat) min_jint)
    return min_jint;
  return (jint) x;
JRT_END


JRT_LEAF(jlong, SharedRuntime::f2l(jfloat  x))
  if (g_isnan(x))
    return 0;
  if (x >= (jfloat) max_jlong)
    return max_jlong;
  if (x <= (jfloat) min_jlong)
    return min_jlong;
  return (jlong) x;
JRT_END


JRT_LEAF(jint, SharedRuntime::d2i(jdouble x))
  if (g_isnan(x))
    return 0;
  if (x >= (jdouble) max_jint)
    return max_jint;
  if (x <= (jdouble) min_jint)
    return min_jint;
  return (jint) x;
JRT_END


JRT_LEAF(jlong, SharedRuntime::d2l(jdouble x))
  if (g_isnan(x))
    return 0;
  if (x >= (jdouble) max_jlong)
    return max_jlong;
  if (x <= (jdouble) min_jlong)
    return min_jlong;
  return (jlong) x;
JRT_END


JRT_LEAF(jfloat, SharedRuntime::d2f(jdouble x))
  return (jfloat)x;
JRT_END


JRT_LEAF(jfloat, SharedRuntime::l2f(jlong x))
  return (jfloat)x;
JRT_END


JRT_LEAF(jdouble, SharedRuntime::l2d(jlong x))
  return (jdouble)x;
JRT_END


/**
 * Throws an java/lang/UnsatisfiedLinkError.  The address of this method is
 * installed in the native function entry of all native Java methods before
 * they get linked to their actual native methods.
 *
 * \note
 * This method actually never gets called!  The reason is because
 * the interpreter's native entries call NativeLookup::lookup() which
 * throws the exception when the lookup fails.  The exception is then
 * caught and forwarded on the return from NativeLookup::lookup() call
 * before the call to the native function.  This might change in the future.
 */
JNI_ENTRY(void*, throw_unsatisfied_link_error(JNIEnv* env, ...))
{
  // We return a bad value here to make sure that the exception is
  // forwarded before we look at the return value.
}
JNI_END

address SharedRuntime::native_method_throw_unsatisfied_link_error_entry() {
  return CAST_FROM_FN_PTR(address, &throw_unsatisfied_link_error);
}

// A simple wrapper class around the calling convention information
// that allows sharing of adapters for the same calling convention.
class AdapterFingerPrint : public CHeapObj<mtCode> {
 private:
  enum {
    _basic_type_bits = 4,
    _basic_type_mask = right_n_bits(_basic_type_bits),
    _basic_types_per_int = BitsPerInt / _basic_type_bits,
    _compact_int_count = 3
  };
  // TO DO:  Consider integrating this with a more global scheme for compressing signatures.
  // For now, 4 bits per components (plus T_VOID gaps after double/long) is not excessive.

  union {
    int  _compact[_compact_int_count];
    int* _fingerprint;
  } _value;
  int _length; // A negative length indicates the fingerprint is in the compact form,
               // Otherwise _value._fingerprint is the array.

  // Remap BasicTypes that are handled equivalently by the adapters.
  // These are correct for the current system but someday it might be
  // necessary to make this mapping platform dependent.
  static int adapter_encoding(BasicType in) {
    switch(in) {
      case T_BOOLEAN:
      case T_BYTE:
      case T_SHORT:
      case T_CHAR:
        // There are all promoted to T_INT in the calling convention
        return T_INT;

      case T_OBJECT:
      case T_ARRAY:
        // In other words, we assume that any register good enough for
        // an int or long is good enough for a managed pointer.
#ifdef _LP64
        return T_LONG;
#else
        return T_INT;
#endif

      case T_INT:
      case T_LONG:
      case T_FLOAT:
      case T_DOUBLE:
      case T_VOID:
        return in;

      default:
        ShouldNotReachHere();
        return T_CONFLICT;
    }
  }

 public:
  AdapterFingerPrint(int total_args_passed, BasicType* sig_bt) {
    // The fingerprint is based on the BasicType signature encoded
    // into an array of ints with eight entries per int.
    int* ptr;
    int len = (total_args_passed + (_basic_types_per_int-1)) / _basic_types_per_int;
    if (len <= _compact_int_count) {
      assert(_compact_int_count == 3, "else change next line");
      _value._compact[0] = _value._compact[1] = _value._compact[2] = 0;
      // Storing the signature encoded as signed chars hits about 98%
      // of the time.
      _length = -len;
      ptr = _value._compact;
    } else {
      _length = len;
      _value._fingerprint = NEW_C_HEAP_ARRAY(int, _length, mtCode);
      ptr = _value._fingerprint;
    }

    // Now pack the BasicTypes with 8 per int
    int sig_index = 0;
    for (int index = 0; index < len; index++) {
      int value = 0;
      for (int byte = 0; byte < _basic_types_per_int; byte++) {
        int bt = ((sig_index < total_args_passed)
                  ? adapter_encoding(sig_bt[sig_index++])
                  : 0);
        assert((bt & _basic_type_mask) == bt, "must fit in 4 bits");
        value = (value << _basic_type_bits) | bt;
      }
      ptr[index] = value;
    }
  }

  ~AdapterFingerPrint() {
    if (_length > 0) {
      FREE_C_HEAP_ARRAY(int, _value._fingerprint, mtCode);
    }
  }

  int value(int index) {
    if (_length < 0) {
      return _value._compact[index];
    }
    return _value._fingerprint[index];
  }
  int length() {
    if (_length < 0) return -_length;
    return _length;
  }

  bool is_compact() {
    return _length <= 0;
  }

  unsigned int compute_hash() {
    int hash = 0;
    for (int i = 0; i < length(); i++) {
      int v = value(i);
      hash = (hash << 8) ^ v ^ (hash >> 5);
    }
    return (unsigned int)hash;
  }

  const char* as_string() {
    stringStream st;
    st.print("0x");
    for (int i = 0; i < length(); i++) {
      st.print("%08x", value(i));
    }
    return st.as_string();
  }

  bool equals(AdapterFingerPrint* other) {
    if (other->_length != _length) {
      return false;
    }
    if (_length < 0) {
      assert(_compact_int_count == 3, "else change next line");
      return _value._compact[0] == other->_value._compact[0] &&
             _value._compact[1] == other->_value._compact[1] &&
             _value._compact[2] == other->_value._compact[2];
    } else {
      for (int i = 0; i < _length; i++) {
        if (_value._fingerprint[i] != other->_value._fingerprint[i]) {
          return false;
        }
      }
    }
    return true;
  }
};


// A hashtable mapping from AdapterFingerPrints to AdapterHandlerEntries
class AdapterHandlerTable : public BasicHashtable<mtCode> {
  friend class AdapterHandlerTableIterator;

 private:

#ifndef PRODUCT
  static int _lookups; // number of calls to lookup
  static int _buckets; // number of buckets checked
  static int _equals;  // number of buckets checked with matching hash
  static int _hits;    // number of successful lookups
  static int _compact; // number of equals calls with compact signature
#endif

  AdapterHandlerEntry* bucket(int i) {
    return (AdapterHandlerEntry*)BasicHashtable<mtCode>::bucket(i);
  }

 public:
  AdapterHandlerTable()
    : BasicHashtable<mtCode>(293, sizeof(AdapterHandlerEntry)) { }

  // Create a new entry suitable for insertion in the table
  AdapterHandlerEntry* new_entry(AdapterFingerPrint* fingerprint, address i2c_entry, address c2i_entry, address c2i_unverified_entry) {
    AdapterHandlerEntry* entry = (AdapterHandlerEntry*)BasicHashtable<mtCode>::new_entry(fingerprint->compute_hash());
    entry->init(fingerprint, i2c_entry, c2i_entry, c2i_unverified_entry);
    return entry;
  }

  // Insert an entry into the table
  void add(AdapterHandlerEntry* entry) {
    int index = hash_to_index(entry->hash());
    add_entry(index, entry);
  }

  void free_entry(AdapterHandlerEntry* entry) {
    entry->deallocate();
    BasicHashtable<mtCode>::free_entry(entry);
  }

  // Find a entry with the same fingerprint if it exists
  AdapterHandlerEntry* lookup(int total_args_passed, BasicType* sig_bt) {
    NOT_PRODUCT(_lookups++);
    AdapterFingerPrint fp(total_args_passed, sig_bt);
    unsigned int hash = fp.compute_hash();
    int index = hash_to_index(hash);
    for (AdapterHandlerEntry* e = bucket(index); e != NULL; e = e->next()) {
      NOT_PRODUCT(_buckets++);
      if (e->hash() == hash) {
        NOT_PRODUCT(_equals++);
        if (fp.equals(e->fingerprint())) {
#ifndef PRODUCT
          if (fp.is_compact()) _compact++;
          _hits++;
#endif
          return e;
        }
      }
    }
    return NULL;
  }

#ifndef PRODUCT
  void print_statistics() {
    ResourceMark rm;
    int longest = 0;
    int empty = 0;
    int total = 0;
    int nonempty = 0;
    for (int index = 0; index < table_size(); index++) {
      int count = 0;
      for (AdapterHandlerEntry* e = bucket(index); e != NULL; e = e->next()) {
        count++;
      }
      if (count != 0) nonempty++;
      if (count == 0) empty++;
      if (count > longest) longest = count;
      total += count;
    }
    tty->print_cr("AdapterHandlerTable: empty %d longest %d total %d average %f",
                  empty, longest, total, total / (double)nonempty);
    tty->print_cr("AdapterHandlerTable: lookups %d buckets %d equals %d hits %d compact %d",
                  _lookups, _buckets, _equals, _hits, _compact);
  }
#endif
};


#ifndef PRODUCT

int AdapterHandlerTable::_lookups;
int AdapterHandlerTable::_buckets;
int AdapterHandlerTable::_equals;
int AdapterHandlerTable::_hits;
int AdapterHandlerTable::_compact;

#endif

// ---------------------------------------------------------------------------
// Implementation of AdapterHandlerLibrary
AdapterHandlerTable* AdapterHandlerLibrary::_adapters = NULL;
AdapterHandlerEntry* AdapterHandlerLibrary::_abstract_method_handler = NULL;
const int AdapterHandlerLibrary_size = 16*K;
BufferBlob* AdapterHandlerLibrary::_buffer = NULL;

BufferBlob* AdapterHandlerLibrary::buffer_blob() {
  // Should be called only when AdapterHandlerLibrary_lock is active.
  if (_buffer == NULL) // Initialize lazily
      _buffer = BufferBlob::create("adapters", AdapterHandlerLibrary_size);
  return _buffer;
}

void AdapterHandlerLibrary::initialize() {
  if (_adapters != NULL) return;
  _adapters = new AdapterHandlerTable();

  // Create a special handler for abstract methods.  Abstract methods
  // are never compiled so an i2c entry is somewhat meaningless, but
  // throw AbstractMethodError just in case.
  // Pass wrong_method_abstract for the c2i transitions to return
  // AbstractMethodError for invalid invocations.
  address wrong_method_abstract = NULL;
  _abstract_method_handler = AdapterHandlerLibrary::new_entry(new AdapterFingerPrint(0, NULL),
                                                              StubRoutines::throw_AbstractMethodError_entry(),
                                                              wrong_method_abstract, wrong_method_abstract);
}

AdapterHandlerEntry* AdapterHandlerLibrary::new_entry(AdapterFingerPrint* fingerprint,
                                                      address i2c_entry,
                                                      address c2i_entry,
                                                      address c2i_unverified_entry) {
  return _adapters->new_entry(fingerprint, i2c_entry, c2i_entry, c2i_unverified_entry);
}

AdapterHandlerEntry* AdapterHandlerLibrary::get_adapter(methodHandle method) {
  D_WARN_Unimplement;
}

address AdapterHandlerEntry::base_address() {
  address base = _i2c_entry;
  if (base == NULL)  base = _c2i_entry;
  assert(base <= _c2i_entry || _c2i_entry == NULL, "");
  assert(base <= _c2i_unverified_entry || _c2i_unverified_entry == NULL, "");
  return base;
}

void AdapterHandlerEntry::relocate(address new_base) {
  address old_base = base_address();
  assert(old_base != NULL, "");
  ptrdiff_t delta = new_base - old_base;
  if (_i2c_entry != NULL)
    _i2c_entry += delta;
  if (_c2i_entry != NULL)
    _c2i_entry += delta;
  if (_c2i_unverified_entry != NULL)
    _c2i_unverified_entry += delta;
  assert(base_address() == new_base, "");
}


void AdapterHandlerEntry::deallocate() {
  delete _fingerprint;
}

/**
 * Create a native wrapper for this native method.  The wrapper converts the
 * Java-compiled calling convention to the native convention, handles
 * arguments, and transitions to native.  On return from the native we transition
 * back to java blocking if a safepoint is in progress.
 */
void AdapterHandlerLibrary::create_native_wrapper(methodHandle method) {
  D_WARN_Unimplement;
}
