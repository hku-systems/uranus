/*
 * Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.
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
#include "utilities/macros.hpp"
#include "memory/allocation.inline.hpp"
#include "prims/jni.h"
#include "prims/jvm.h"
#include "runtime/globals.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "runtime/synchronizer.hpp"
#include "utilities/copy.hpp"

#include "sun_misc_Unsafe.h"

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

/*
 *      Implementation of class sun.misc.Unsafe
 */

#define MAX_OBJECT_SIZE \
  ( arrayOopDesc::header_size(T_DOUBLE) * HeapWordSize \
    + ((julong)max_jint * sizeof(double)) )

#define UNSAFE_ENTRY(result_type, header) \
  extern "C" { \
  result_type header {\
  JavaThread* thread = JavaThread::thread_from_jni_environment(env); \
  Thread* THREAD = thread;

#define UNSAFE_END }}

// Can't use UNSAFE_LEAF because it has the signature of a straight
// call into the runtime (just like JVM_LEAF, funny that) but it's
// called like a Java Native and thus the wrapper built for it passes
// arguments like a JNI call.  It expects those arguments to be popped
// from the stack on Intel like all good JNI args are, and adjusts the
// stack according.  Since the JVM_LEAF call expects no extra
// arguments the stack isn't popped in the C code, is pushed by the
// wrapper and we get sick.
//#define UNSAFE_LEAF(result_type, header) \
//  JVM_LEAF(result_type, header)

#define UnsafeWrapper(arg) /*nothing, for the present*/


inline void* addr_from_java(jlong addr) {
  // This assert fails in a variety of ways on 32-bit systems.
  // It is impossible to predict whether native code that converts
  // pointers to longs will sign-extend or zero-extend the addresses.
  //assert(addr == (uintptr_t)addr, "must not be odd high bits");
  return (void*)(uintptr_t)addr;
}

inline jlong addr_to_java(void* p) {
  assert(p == (void*)(uintptr_t)p, "must not be odd high bits");
  return (uintptr_t)p;
}


// Note: The VM's obj_field and related accessors use byte-scaled
// ("unscaled") offsets, just as the unsafe methods do.

// However, the method Unsafe.fieldOffset explicitly declines to
// guarantee this.  The field offset values manipulated by the Java user
// through the Unsafe API are opaque cookies that just happen to be byte
// offsets.  We represent this state of affairs by passing the cookies
// through conversion functions when going between the VM and the Unsafe API.
// The conversion functions just happen to be no-ops at present.

inline jlong field_offset_to_byte_offset(jlong field_offset) {
  return field_offset;
}

inline jlong field_offset_from_byte_offset(jlong byte_offset) {
  return byte_offset;
}

inline jint invocation_key_from_method_slot(jint slot) {
  return slot;
}

inline jint invocation_key_to_method_slot(jint key) {
  return key;
}

inline void* index_oop_from_field_offset_long(oop p, jlong field_offset) {
  jlong byte_offset = field_offset_to_byte_offset(field_offset);
#ifdef ASSERT
  if (p != NULL) {
    assert(byte_offset >= 0 && byte_offset <= (jlong)MAX_OBJECT_SIZE, "sane offset");
    if (byte_offset == (jint)byte_offset) {
      void* ptr_plus_disp = (address)p + byte_offset;
      assert((void*)p->obj_field_addr<oop>((jint)byte_offset) == ptr_plus_disp,
             "raw [ptr+disp] must be consistent with oop::field_base");
    }
    jlong p_size = HeapWordSize * (jlong)(p->size());
    assert(byte_offset < p_size, err_msg("Unsafe access: offset " INT64_FORMAT " > object's size " INT64_FORMAT, byte_offset, p_size));
  }
#endif
  if (sizeof(char*) == sizeof(jint))    // (this constant folds!)
    return (address)p + (jint) byte_offset;
  else
    return (address)p +        byte_offset;
}

// Externally callable versions:
// (Use these in compiler intrinsics which emulate unsafe primitives.)
jlong Unsafe_field_offset_to_byte_offset(jlong field_offset) {
  return field_offset;
}
jlong Unsafe_field_offset_from_byte_offset(jlong byte_offset) {
  return byte_offset;
}
jint Unsafe_invocation_key_from_method_slot(jint slot) {
  return invocation_key_from_method_slot(slot);
}
jint Unsafe_invocation_key_to_method_slot(jint key) {
  return invocation_key_to_method_slot(key);
}


///// Data in the Java heap.

#define truncate_jboolean(x) ((x) & 1)
#define truncate_jbyte(x) (x)
#define truncate_jshort(x) (x)
#define truncate_jchar(x) (x)
#define truncate_jint(x) (x)
#define truncate_jlong(x) (x)
#define truncate_jfloat(x) (x)
#define truncate_jdouble(x) (x)

#define GET_FIELD(obj, offset, type_name, v) \
  oop p = JNIHandles::resolve(obj); \
  type_name v = *(type_name*)index_oop_from_field_offset_long(p, offset)

#define SET_FIELD(obj, offset, type_name, x) \
  oop p = JNIHandles::resolve(obj); \
  *(type_name*)index_oop_from_field_offset_long(p, offset) = truncate_##type_name(x)

#define GET_FIELD_VOLATILE(obj, offset, type_name, v) \
  oop p = JNIHandles::resolve(obj); \
  if (support_IRIW_for_not_multiple_copy_atomic_cpu) { \
    OrderAccess::fence(); \
  } \
  volatile type_name v = OrderAccess::load_acquire((volatile type_name*)index_oop_from_field_offset_long(p, offset));

#define SET_FIELD_VOLATILE(obj, offset, type_name, x) \
  oop p = JNIHandles::resolve(obj); \
  OrderAccess::release_store_fence((volatile type_name*)index_oop_from_field_offset_long(p, offset), truncate_##type_name(x));

// Macros for oops that check UseCompressedOops

#define GET_OOP_FIELD(obj, offset, v) \
  oop p = JNIHandles::resolve(obj);   \
  oop v;                              \
  if (UseCompressedOops) {            \
    narrowOop n = *(narrowOop*)index_oop_from_field_offset_long(p, offset); \
    v = oopDesc::decode_heap_oop(n);                                \
  } else {                            \
    v = *(oop*)index_oop_from_field_offset_long(p, offset);                 \
  }

UNSAFE_ENTRY(jboolean, Unsafe_CompareAndSwapObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject e_h, jobject x_h))
    UnsafeWrapper("Unsafe_CompareAndSwapObject");
    oop x = JNIHandles::resolve(x_h);
    oop e = JNIHandles::resolve(e_h);
    oop p = JNIHandles::resolve(obj);
    HeapWord* addr = (HeapWord *)index_oop_from_field_offset_long(p, offset);
    oop res = oopDesc::atomic_compare_exchange_oop(x, addr, e, true);
    jboolean success  = (res == e);
    if (success)
        update_barrier_set((void*)addr, x);
    return success;
UNSAFE_END

UNSAFE_ENTRY(jboolean, Unsafe_CompareAndSwapInt(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jint e, jint x))
    UnsafeWrapper("Unsafe_CompareAndSwapInt");
    oop p = JNIHandles::resolve(obj);
    jint* addr = (jint *) index_oop_from_field_offset_long(p, offset);
    return (jint)(Atomic::cmpxchg(x, addr, e)) == e;
UNSAFE_END

UNSAFE_ENTRY(jboolean, Unsafe_CompareAndSwapLong(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jlong e, jlong x))
    UnsafeWrapper("Unsafe_CompareAndSwapLong");
    oop p = JNIHandles::resolve(obj);
    jlong* addr = (jlong*)(index_oop_from_field_offset_long(p, offset));
#ifdef SUPPORTS_NATIVE_CX8
    jboolean v = (jlong)(Atomic::cmpxchg(x, addr, e)) == e;
    return v;
#else
    if (VM_Version::supports_cx8())
    return (jlong)(Atomic::cmpxchg(x, addr, e)) == e;
  else {
    jboolean success = false;
    MutexLockerEx mu(UnsafeJlong_lock, Mutex::_no_safepoint_check_flag);
    jlong val = Atomic::load(addr);
    if (val == e) { Atomic::store(x, addr); success = true; }
    return success;
  }
#endif
UNSAFE_END

UNSAFE_ENTRY(jobject, Unsafe_GetObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset))
    UnsafeWrapper("Unsafe_GetObject");
    GET_OOP_FIELD(obj, offset, v)
    jobject ret = JNIHandles::make_local(env, v);
    return ret;
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject x_h))
    UnsafeWrapper("Unsafe_SetObject");
    oop x = JNIHandles::resolve(x_h);
    oop p = JNIHandles::resolve(obj);
    oop_store((narrowOop*)index_oop_from_field_offset_long(p, offset), x);
    oop_store((oop*)index_oop_from_field_offset_long(p, offset), x);
UNSAFE_END

UNSAFE_ENTRY(jobject, Unsafe_GetObjectVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset))
    UnsafeWrapper("Unsafe_GetObjectVolatile");
    oop p = JNIHandles::resolve(obj);
    void* addr = index_oop_from_field_offset_long(p, offset);
    volatile oop v;
    (void)const_cast<oop&>(v = *(volatile oop*) addr);
    OrderAccess::acquire();
    return JNIHandles::make_local(env, v);
UNSAFE_END

#ifndef SUPPORTS_NATIVE_CX8

UNSAFE_ENTRY(jlong, Unsafe_GetLongVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset))
    UnsafeWrapper("Unsafe_GetLongVolatile");
    {
        if (VM_Version::supports_cx8()) {
            GET_FIELD_VOLATILE(obj, offset, jlong, v);
            return v;
        }
        else {
            Handle p (THREAD, JNIHandles::resolve(obj));
            jlong* addr = (jlong*)(index_oop_from_field_offset_long(p(), offset));
            jlong value = Atomic::load(addr);
            return value;
        }
    }
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetLongVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jlong x))
    UnsafeWrapper("Unsafe_SetLongVolatile");
    {
        if (VM_Version::supports_cx8()) {
            SET_FIELD_VOLATILE(obj, offset, jlong, x);
        }
        else {
            Handle p (THREAD, JNIHandles::resolve(obj));
            jlong* addr = (jlong*)(index_oop_from_field_offset_long(p(), offset));
            Atomic::store(x, addr);
        }
    }
UNSAFE_END

#endif // not SUPPORTS_NATIVE_CX8

#define DEFINE_GETSETOOP(jboolean, Boolean) \
 \
UNSAFE_ENTRY(jboolean, Unsafe_Get##Boolean##140(JNIEnv *env, jobject unsafe, jobject obj, jint offset)) \
  UnsafeWrapper("Unsafe_Get"#Boolean); \
  if (obj == NULL)  ENCLAVE_THROW_0(EnclaveException::java_lang_NullPointerException); \
  GET_FIELD(obj, offset, jboolean, v); \
  return v; \
UNSAFE_END \
 \
UNSAFE_ENTRY(void, Unsafe_Set##Boolean##140(JNIEnv *env, jobject unsafe, jobject obj, jint offset, jboolean x)) \
  UnsafeWrapper("Unsafe_Set"#Boolean); \
  if (obj == NULL)  ENCLAVE_THROW(EnclaveException::java_lang_NullPointerException); \
  SET_FIELD(obj, offset, jboolean, x); \
UNSAFE_END \
 \
UNSAFE_ENTRY(jboolean, Unsafe_Get##Boolean(JNIEnv *env, jobject unsafe, jobject obj, jlong offset)) \
  UnsafeWrapper("Unsafe_Get"#Boolean); \
  GET_FIELD(obj, offset, jboolean, v); \
  return v; \
UNSAFE_END \
 \
UNSAFE_ENTRY(void, Unsafe_Set##Boolean(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jboolean x)) \
  UnsafeWrapper("Unsafe_Set"#Boolean); \
  SET_FIELD(obj, offset, jboolean, x); \
UNSAFE_END \
 \
// END DEFINE_GETSETOOP.

DEFINE_GETSETOOP(jboolean, Boolean)
DEFINE_GETSETOOP(jbyte, Byte)
DEFINE_GETSETOOP(jshort, Short);
DEFINE_GETSETOOP(jchar, Char);
DEFINE_GETSETOOP(jint, Int);
DEFINE_GETSETOOP(jlong, Long);
DEFINE_GETSETOOP(jfloat, Float);
DEFINE_GETSETOOP(jdouble, Double);

#undef DEFINE_GETSETOOP

#define DEFINE_GETSETOOP_VOLATILE(jboolean, Boolean) \
 \
UNSAFE_ENTRY(jboolean, Unsafe_Get##Boolean##Volatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset)) \
  UnsafeWrapper("Unsafe_Get"#Boolean); \
  GET_FIELD_VOLATILE(obj, offset, jboolean, v); \
  return v; \
UNSAFE_END \
 \
UNSAFE_ENTRY(void, Unsafe_Set##Boolean##Volatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jboolean x)) \
  UnsafeWrapper("Unsafe_Set"#Boolean); \
  SET_FIELD_VOLATILE(obj, offset, jboolean, x); \
UNSAFE_END \
 \
// END DEFINE_GETSETOOP_VOLATILE.

DEFINE_GETSETOOP_VOLATILE(jboolean, Boolean)
DEFINE_GETSETOOP_VOLATILE(jbyte, Byte)
DEFINE_GETSETOOP_VOLATILE(jshort, Short);
DEFINE_GETSETOOP_VOLATILE(jchar, Char);
DEFINE_GETSETOOP_VOLATILE(jint, Int);
DEFINE_GETSETOOP_VOLATILE(jfloat, Float);
DEFINE_GETSETOOP_VOLATILE(jdouble, Double);

#ifdef SUPPORTS_NATIVE_CX8
DEFINE_GETSETOOP_VOLATILE(jlong, Long);
#endif

#undef DEFINE_GETSETOOP_VOLATILE

UNSAFE_ENTRY(void, Unsafe_SetObjectVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject x_h))
    UnsafeWrapper("Unsafe_SetObjectVolatile");
    oop x = JNIHandles::resolve(x_h);
    oop p = JNIHandles::resolve(obj);
    void* addr = index_oop_from_field_offset_long(p, offset);
    OrderAccess::release();
    oop_store((oop*)addr, x);
    OrderAccess::fence();
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_CopyMemory(JNIEnv *env, jobject unsafe, jlong srcAddr, jlong dstAddr, jlong size))
    UnsafeWrapper("Unsafe_CopyMemory");
    if (size == 0) {
        return;
    }
    size_t sz = (size_t)size;
    if (sz != (julong)size || size < 0) {
        ENCLAVE_THROW(EnclaveException::java_lang_IllegalArgumentException);
    }
    void* src = addr_from_java(srcAddr);
    void* dst = addr_from_java(dstAddr);
    printf(D_INFO("UnsafeCopy")" %lx -> %lx", (intptr_t)src, (intptr_t)dst);
    Copy::conjoint_memory_atomic(src, dst, sz);
UNSAFE_END
