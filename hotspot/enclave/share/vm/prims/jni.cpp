/*
 * Copyright (c) 1997, 2016, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2012 Red Hat, Inc.
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
#include "classfile/altHashing.hpp"
#include "classfile/classLoader.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "utilities/macros.hpp"
#include "memory/allocation.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/universe.inline.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/instanceOop.hpp"
#include "oops/markOop.hpp"
#include "oops/method.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/objArrayOop.hpp"
#include "oops/oop.inline.hpp"
#include "oops/symbol.hpp"
#include "oops/typeArrayKlass.hpp"
#include "oops/typeArrayOop.hpp"
#include "prims/jni.h"
#include "prims/jvm.h"
#include "runtime/fieldDescriptor.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/signature.hpp"
#include "runtime/thread.inline.hpp"
#ifdef TARGET_OS_FAMILY_linux
# include "os_linux.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_solaris
# include "os_solaris.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_windows
# include "os_windows.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_bsd
# include "os_bsd.inline.hpp"
#endif

static jint CurrentVersion = JNI_VERSION_1_8;

#define JNI_ENTRY(ret, header) \
  extern "C" { \
  ret header { \
  JavaThread* thread=JavaThread::thread_from_jni_environment(env); \
  JavaThread* THREAD = thread;

#define JNI_END }}

JNI_ENTRY(jclass, jni_GetObjectClass(JNIEnv *env, jobject obj))
  Klass* k = JNIHandles::resolve_non_null(obj)->klass();
  jclass ret =
          (jclass) JNIHandles::make_local(env, k->java_mirror());
  return ret;
JNI_END

JNI_ENTRY(jclass, jni_GetSuperclass(JNIEnv *env, jclass sub))
  D_WARN_Unstable;
  jclass obj = NULL;

  oop mirror = JNIHandles::resolve_non_null(sub);
  // primitive classes return NULL
  if (java_lang_Class::is_primitive(mirror)) return NULL;

  // Rules of Class.getSuperClass as implemented by KLass::java_super:
  // arrays return Object
  // interfaces return NULL
  // proper classes return Klass::super()
  Klass* k = java_lang_Class::as_Klass(mirror);
  if (k->is_interface()) return NULL;

  // return mirror for superclass
  Klass* super;
  if (Klass::layout_helper_is_array(k->_layout_helper)) {
      super = EnclaveMemory::wk_classes[SystemDictionary::Object_klass_knum];
      printf("klass %s %lx\n", k->name()->as_C_string(), (intptr_t)super);
  } else {
      super = k->super();
      printf("instance klass %s\n", k->name()->as_C_string());
  }
  // super2 is the value computed by the compiler's getSuperClass intrinsic:
  debug_only(Klass* super2 = ( k->oop_is_array()
                               ? SystemDictionary::Object_klass()
                               : k->super() ) );
  assert(super == super2,
         "java_super computation depends on interface, array, other super");
  obj = (super == NULL) ? NULL : (jclass) JNIHandles::make_local(super->java_mirror());
  return obj;
JNI_END


JNI_ENTRY(jboolean, jni_IsAssignableFrom(JNIEnv *env, jclass sub, jclass super))
  D_WARN_Unstable;
// order should be exchanged
  oop sub_mirror   = JNIHandles::resolve_non_null(super);
  oop super_mirror = JNIHandles::resolve_non_null(sub);
  if (java_lang_Class::is_primitive(sub_mirror) ||
    java_lang_Class::is_primitive(super_mirror)) {
    jboolean ret = (sub_mirror == super_mirror);

    return ret;
  }
  Klass* sub_klass   = java_lang_Class::as_Klass(sub_mirror);
  Klass* super_klass = java_lang_Class::as_Klass(super_mirror);
  //  printf(D_WARN("Native")" %s -> %s\n", sub_klass->name()->as_C_string(), super_klass->name()->as_C_string());
  assert(sub_klass != NULL && super_klass != NULL, "invalid arguments to jni_IsAssignableFrom");
  jboolean ret = sub_klass->is_subtype_of(super_klass) ?
                 JNI_TRUE : JNI_FALSE;
  return ret;
JNI_END


JNI_ENTRY(jboolean, jni_IsInstanceOf(JNIEnv *env, jobject obj, jclass clazz))
  jboolean ret = JNI_TRUE;
  if (obj != NULL) {
    ret = JNI_FALSE;
    Klass* k = java_lang_Class::as_Klass(
          JNIHandles::resolve_non_null(clazz));
    if (k != NULL) {
      ret = JNIHandles::resolve_non_null(obj)->is_a(k) ? JNI_TRUE : JNI_FALSE;
    }
  }
return ret;
JNI_END

JNI_ENTRY(jsize, jni_GetArrayLength(JNIEnv *env, jarray array))
  arrayOop a = arrayOop(JNIHandles::resolve_non_null(array));
  assert(a->is_array(), "must be array");
  jsize ret = a->length();
  return ret;
JNI_END

#define DEFINE_GETSCALARARRAYREGION(ElementTag,ElementType,Result, Tag) \
\
JNI_ENTRY(void, \
jni_Get##Result##ArrayRegion(JNIEnv *env, ElementType##Array array, jsize start, \
             jsize len, ElementType *buf)) \
  typeArrayOop src = typeArrayOop(JNIHandles::resolve_non_null(array)); \
  if (start < 0 || len < 0 || ((unsigned int)start + (unsigned int)len > (unsigned int)src->length())) { \
    ENCLAVE_THROW(EnclaveException::java_lang_ArrayIndexOutOfBoundsException); \
  } else { \
    if (len > 0) { \
      int sc = TypeArrayKlass::cast(src->klass())->log2_element_size(); \
      memcpy((u_char*) buf, \
             (u_char*) src->Tag##_at_addr(start), \
             len << sc);                          \
    } \
  } \
JNI_END

DEFINE_GETSCALARARRAYREGION(T_BOOLEAN, jboolean,Boolean, bool)
DEFINE_GETSCALARARRAYREGION(T_BYTE,    jbyte,   Byte,    byte)
DEFINE_GETSCALARARRAYREGION(T_SHORT,   jshort,  Short,   short)
DEFINE_GETSCALARARRAYREGION(T_CHAR,    jchar,   Char,    char)
DEFINE_GETSCALARARRAYREGION(T_INT,     jint,    Int,     int)
DEFINE_GETSCALARARRAYREGION(T_LONG,    jlong,   Long,    long)
DEFINE_GETSCALARARRAYREGION(T_FLOAT,   jfloat,  Float,   float)
DEFINE_GETSCALARARRAYREGION(T_DOUBLE,  jdouble, Double,  double)


#define RESOLVE_HANDLE(handle) ((oop)handle)

#define DEFINE_SETSCALARARRAYREGION(ElementTag,ElementType,Result, Tag) \
\
JNI_ENTRY(void, \
jni_Set##Result##ArrayRegion(JNIEnv *env, ElementType##Array array, jsize start, \
             jsize len, const ElementType *buf)) \
  typeArrayOop dst = typeArrayOop(RESOLVE_HANDLE(array)); \
  if (start < 0 || len < 0 || ((unsigned int)start + (unsigned int)len > (unsigned int)dst->length())) { \
    ENCLAVE_THROW(EnclaveException::java_lang_ArrayIndexOutOfBoundsException); \
  } else { \
    if (len > 0) { \
      int sc = TypeArrayKlass::cast(dst->klass())->log2_element_size(); \
      memcpy((u_char*) dst->Tag##_at_addr(start), \
             (u_char*) buf, \
             len << sc);    \
    } \
  } \
JNI_END

DEFINE_SETSCALARARRAYREGION(T_BOOLEAN, jboolean, Boolean, bool)
DEFINE_SETSCALARARRAYREGION(T_BYTE,    jbyte,    Byte,    byte)
DEFINE_SETSCALARARRAYREGION(T_SHORT,   jshort,   Short,   short)
DEFINE_SETSCALARARRAYREGION(T_CHAR,    jchar,    Char,    char)
DEFINE_SETSCALARARRAYREGION(T_INT,     jint,     Int,     int)
DEFINE_SETSCALARARRAYREGION(T_LONG,    jlong,    Long,    long)
DEFINE_SETSCALARARRAYREGION(T_FLOAT,   jfloat,   Float,   float)
DEFINE_SETSCALARARRAYREGION(T_DOUBLE,  jdouble,  Double,  double)

static char* get_bad_address() {
  static char* bad_address = NULL;
  if (bad_address == NULL) {
    size_t size = os::vm_allocation_granularity();
    bad_address = os::reserve_memory(size);
    if (bad_address != NULL) {
      os::protect_memory(bad_address, size, os::MEM_PROT_READ,
              /*is_committed*/false);
    }
  }
  return bad_address;
}

#define DEFINE_GETSCALARARRAYELEMENTS(ElementTag,ElementType,Result, Tag) \
\
JNI_ENTRY(ElementType*, \
          jni_Get##Result##ArrayElements(JNIEnv *env, ElementType##Array array, jboolean *isCopy)) \
  /* allocate an chunk of memory in c land */ \
  typeArrayOop a = typeArrayOop(JNIHandles::resolve_non_null(array)); \
  ElementType* result; \
  int len = a->length(); \
  if (len == 0) { \
    /* Empty array: legal but useless, can't return NULL. \
     * Return a pointer to something useless. \
     * Avoid asserts in typeArrayOop. */ \
    result = (ElementType*)get_bad_address(); \
  } else { \
    /* JNI Specification states return NULL on OOM */                    \
    result = NEW_C_HEAP_ARRAY_RETURN_NULL(ElementType, len, mtInternal); \
    if (result != NULL) {                                                \
      /* copy the array to the c chunk */                                \
      memcpy(result, a->Tag##_at_addr(0), sizeof(ElementType)*len);      \
      if (isCopy) {                                                      \
        *isCopy = JNI_TRUE;                                              \
      }                                                                  \
    }                                                                    \
  } \
  return result; \
JNI_END

DEFINE_GETSCALARARRAYELEMENTS(T_BOOLEAN, jboolean, Boolean, bool)
DEFINE_GETSCALARARRAYELEMENTS(T_BYTE,    jbyte,    Byte,    byte)
DEFINE_GETSCALARARRAYELEMENTS(T_SHORT,   jshort,   Short,   short)
DEFINE_GETSCALARARRAYELEMENTS(T_CHAR,    jchar,    Char,    char)
DEFINE_GETSCALARARRAYELEMENTS(T_INT,     jint,     Int,     int)
DEFINE_GETSCALARARRAYELEMENTS(T_LONG,    jlong,    Long,    long)
DEFINE_GETSCALARARRAYELEMENTS(T_FLOAT,   jfloat,   Float,   float)
DEFINE_GETSCALARARRAYELEMENTS(T_DOUBLE,  jdouble,  Double,  double)

#define DEFINE_RELEASESCALARARRAYELEMENTS(ElementTag,ElementType,Result,Tag) \
\
JNI_ENTRY(void, \
          jni_Release##Result##ArrayElements(JNIEnv *env, ElementType##Array array, \
                                             ElementType *buf, jint mode)) \
  typeArrayOop a = typeArrayOop(JNIHandles::resolve_non_null(array)); \
  int len = a->length(); \
  if (len != 0) {   /* Empty array:  nothing to free or copy. */  \
    if ((mode == 0) || (mode == JNI_COMMIT)) { \
      memcpy(a->Tag##_at_addr(0), buf, sizeof(ElementType)*len); \
    } \
    if ((mode == 0) || (mode == JNI_ABORT)) { \
      FreeHeap(buf); \
    } \
  } \
JNI_END

DEFINE_RELEASESCALARARRAYELEMENTS(T_BOOLEAN, jboolean, Boolean, bool)
DEFINE_RELEASESCALARARRAYELEMENTS(T_BYTE,    jbyte,    Byte,    byte)
DEFINE_RELEASESCALARARRAYELEMENTS(T_SHORT,   jshort,   Short,   short)
DEFINE_RELEASESCALARARRAYELEMENTS(T_CHAR,    jchar,    Char,    char)
DEFINE_RELEASESCALARARRAYELEMENTS(T_INT,     jint,     Int,     int)
DEFINE_RELEASESCALARARRAYELEMENTS(T_LONG,    jlong,    Long,    long)
DEFINE_RELEASESCALARARRAYELEMENTS(T_FLOAT,   jfloat,   Float,   float)
DEFINE_RELEASESCALARARRAYELEMENTS(T_DOUBLE,  jdouble,  Double,  double)

#define JNI_CALL_SINGLE_ENUM(pre, ElemType, su) jni_##pre##ElemType##Array##su
#define JNI_CALL_ENUM(pre, su) \
  JNI_CALL_SINGLE_ENUM(pre, Boolean, su), \
  JNI_CALL_SINGLE_ENUM(pre, Byte, su), \
  JNI_CALL_SINGLE_ENUM(pre, Char, su), \
  JNI_CALL_SINGLE_ENUM(pre, Short, su), \
  JNI_CALL_SINGLE_ENUM(pre, Int, su), \
  JNI_CALL_SINGLE_ENUM(pre, Long, su), \
  JNI_CALL_SINGLE_ENUM(pre, Float, su), \
  JNI_CALL_SINGLE_ENUM(pre, Double, su)

struct JNIEnv jni = {
        jni_GetSuperclass,
        jni_IsAssignableFrom,
        jni_GetObjectClass,
        jni_IsInstanceOf,
        jni_GetArrayLength,

        JNI_CALL_ENUM(Get, Region),
        JNI_CALL_ENUM(Set, Region),
        JNI_CALL_ENUM(Get, Elements),
        JNI_CALL_ENUM(Release, Elements),
};

JNIEnv* jni_functions() {
  return &jni;
}