/*
 * Copyright (c) 1997, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "runtime/synchronizer.hpp"
#include "precompiled.hpp"
#include "classfile/classLoader.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#if INCLUDE_CDS
#include "classfile/systemDictionaryShared.hpp"
#endif
#include "classfile/vmSymbols.hpp"
#include "interpreter/bytecode.hpp"
#include "memory/universe.inline.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/method.hpp"
#include "prims/jvm.h"
#include "runtime/handles.inline.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "runtime/os.hpp"
#include "utilities/copy.hpp"
#include "utilities/top.hpp"
#include "utilities/utf8.hpp"
#ifdef TARGET_OS_FAMILY_linux
# include "jvm_linux.h"
#endif
#ifdef TARGET_OS_FAMILY_solaris
# include "jvm_solaris.h"
#endif
#ifdef TARGET_OS_FAMILY_windows
# include "jvm_windows.h"
#endif
#ifdef TARGET_OS_FAMILY_aix
# include "jvm_aix.h"
#endif
#ifdef TARGET_OS_FAMILY_bsd
# include "jvm_bsd.h"
#endif


#define JVM_ENTRY(ret, header) \
  extern "C" { \
  ret header { \
  JavaThread* thread=JavaThread::thread_from_jni_environment(env); \
  JavaThread* THREAD = thread;

#define JVM_END }}

#define JVMWrapper(msg)

extern "C" {

ATTRIBUTE_PRINTF(3, 0)
int jio_vsnprintf(char *str, size_t count, const char *fmt, va_list args) {
  // see bug 4399518, 4417214
  if ((intptr_t)count <= 0) return -1;

  int result = vsnprintf(str, count, fmt, args);
  // Note: on truncation vsnprintf(3) on Unix returns number of
  // characters which would have been written had the buffer been large
  // enough; on Windows, it returns -1. We handle both cases here and
  // always return -1, and perform null termination.
  if ((result > 0 && (size_t)result >= count) || result == -1) {
    str[count - 1] = '\0';
    result = -1;
  }

  return result;
}

ATTRIBUTE_PRINTF(3, 0)
int jio_snprintf(char *str, size_t count, const char *fmt, ...) {
  va_list args;
  int len;
  va_start(args, fmt);
  len = jio_vsnprintf(str, count, fmt, args);
          va_end(args);
  return len;
}

ATTRIBUTE_PRINTF(2,3)
int jio_fprintf(FILE* f, const char *fmt, ...) {
  int len;
  va_list args;
  va_start(args, fmt);
  len = jio_vfprintf(f, fmt, args);
          va_end(args);
  return len;
}

ATTRIBUTE_PRINTF(2, 0)
int jio_vfprintf(FILE* f, const char *fmt, va_list args) {
  return vfprintf(f, fmt, args);
}

// HotSpot specific jio method
void jio_print(const char* s) {
  // Try to make this function as atomic as possible.
    // Make an unused local variable to avoid warning from gcc 4.x compiler.
  size_t count = ::write(stdout, s, (int)strlen(s));
}

} // Extern C

void array_set(jvalue* value, arrayOop a, int index, BasicType value_type, TRAPS) {
  if (!a->is_within_bounds(index)) {
    THROW(vmSymbols::java_lang_IndexOutOfBoundsException());
  }
  if (a->is_objArray()) {
    if (value_type == T_OBJECT) {
      oop obj = (oop) value->l;
      if (obj != NULL) {
        Klass* element_klass = ObjArrayKlass::cast(a->klass())->element_klass();
        if (!obj->is_a(element_klass)) {
          THROW(vmSymbols::java_lang_IllegalArgumentException());
        }
      }
      objArrayOop(a)->obj_at_put(index, obj);
    }
  } else {
    assert(a->is_typeArray(), "just checking");
    BasicType array_type = TypeArrayKlass::cast(a->klass())->element_type();
    printf("type %d, %d\n", array_type, value_type);
    if (array_type != value_type) {
      // The widen operation can potentially throw an exception, but cannot block,
      // so typeArrayOop a is safe if the call succeeds.
//      widen(value, value_type, array_type, CHECK);
    }
    switch (array_type) {
      case T_BOOLEAN:
        typeArrayOop(a)->bool_at_put(index, value->z);
            break;
      case T_CHAR:
        typeArrayOop(a)->char_at_put(index, value->c);
            break;
      case T_FLOAT:
        typeArrayOop(a)->float_at_put(index, value->f);
            break;
      case T_DOUBLE:
        typeArrayOop(a)->double_at_put(index, value->d);
            break;
      case T_BYTE:
        typeArrayOop(a)->byte_at_put(index, value->b);
            break;
      case T_SHORT:
        typeArrayOop(a)->short_at_put(index, value->s);
            break;
      case T_INT:
        typeArrayOop(a)->int_at_put(index, value->i);
            break;
      case T_LONG:
        typeArrayOop(a)->long_at_put(index, value->j);
            break;
      default:
      THROW(vmSymbols::java_lang_IllegalArgumentException());
    }
  }
}

static inline arrayOop check_array(JNIEnv *env, jobject arr, bool type_array_only, TRAPS) {
  if (arr == NULL) {
    THROW_0(vmSymbols::java_lang_NullPointerException());
  }
  oop a = JNIHandles::resolve_non_null(arr);
  if (!a->is_array() || (type_array_only && !a->is_typeArray())) {
    THROW_0(vmSymbols::java_lang_IllegalArgumentException());;
  }
  return arrayOop(a);
}

JVM_ENTRY(void, JVM_ArrayCopy(JNIEnv *env, jclass ignored, jobject src, jint src_pos,
                              jobject dst, jint dst_pos, jint length))
// Check if we have null pointers
  if (src == NULL || dst == NULL) {
    THROW(vmSymbols::java_lang_NullPointerException());
  }
  arrayOop s = arrayOop(JNIHandles::resolve_non_null(src));
  arrayOop d = arrayOop(JNIHandles::resolve_non_null(dst));
  EnclaveMemory::copy_array(s, src_pos, d, dst_pos, length);
JVM_END

JVM_ENTRY(jobject, JVM_Clone(JNIEnv* env, jobject handle))
  JVMWrapper("JVM_Clone");

  //TODO-jianyu: fake implementation of clone
  Handle obj(THREAD, JNIHandles::resolve_non_null(handle));
  Klass* klass = obj->klass();

  const int size = obj->size();
  oop new_obj_oop = NULL;

  if (obj->is_array()) {
      const int length = ((arrayOop)obj())->length();
      if (EnclaveMemory::is_type_oop(klass)) {
          new_obj_oop = (oop)EnclaveMemory::static_klass_type_array(THREAD, klass, length);
      } else {
          new_obj_oop = (oop)EnclaveMemory::static_klass_obj_array(THREAD, klass, length);
      }
  } else {
      new_obj_oop = (oop)EnclaveMemory::static_vm_new_obj(THREAD, klass);
  }
//    printf(D_INFO("Native") " copy from %lx to %lx\n", (intptr_t)obj(), (intptr_t)new_obj_oop);
  Copy::conjoint_jlongs_atomic((jlong*)obj(), (jlong*)new_obj_oop,
                               (size_t)align_object_size(size) / HeapWordsPerLong);

  return JNIHandles::make_local(env, new_obj_oop);
JVM_END

JVM_ENTRY(jint, JVM_IHashCode(JNIEnv* env, jobject handle))
  JVMWrapper("JVM_IHashCode");
  // as implemented in the classic virtual machine; return 0 if object is NULL
  return handle == NULL ? 0 : ObjectSynchronizer::FastHashCode (THREAD, JNIHandles::resolve_non_null(handle)) ;
JVM_END

JVM_ENTRY(void, JVM_MonitorNotifyAll(JNIEnv* env, jobject handle))
  D_WARN_Unimplement;
JVM_END

JVM_ENTRY(void, JVM_MonitorNotify(JNIEnv* env, jobject handle))
  D_WARN_Unimplement;
JVM_END

JVM_ENTRY(jobject, JVM_EnclaveNewArray(JNIEnv *env, jclass mirror, jclass eltClass, jint length))
  JVMWrapper("JVM_EnclaveNewArray");
  oop element_mirror = JNIHandles::resolve(eltClass);
  address ar = EnclaveMemory::static_klass_new_array((JavaThread *) THREAD, element_mirror, length);
  return JNIHandles::make_local(env, (oop)ar);
JVM_END

JVM_ENTRY(jobject, JVM_EnclaveNewMultiArray(JNIEnv *env, jclass mirror,jclass eltClass, jintArray dim))
  JVMWrapper("JVM_NewMultiArray");
  arrayOop dim_array = check_array(env, dim, true, CHECK_NULL);
  oop element_mirror = JNIHandles::resolve(eltClass);
  oop result = (oop) EnclaveMemory::static_klass_multi_array((JavaThread *) THREAD, element_mirror, dim_array);
    return JNIHandles::make_local(env, result);
JVM_END

JVM_ENTRY(void, JVM_EnclaveSetArrayElement(JNIEnv *env, jclass ignore, jobject arr, jint index, jobject val))
  JVMWrapper("JVM_SetArrayElement");
  arrayOop a = check_array(env, arr, false, CHECK);
  oop box = JNIHandles::resolve(val);
  jvalue value;
  value.i = 0; // to initialize value before getting used in CHECK
  BasicType value_type;
  if (a->is_objArray()) {
    // Make sure we do no unbox e.g. java/lang/Integer instances when storing into an object array
    value.l = (jobject) box;
    value_type = T_OBJECT;
  } else {
    if (box == NULL) {
      THROW(vmSymbols::java_lang_IllegalArgumentException());
    }
    value_type = java_lang_boxing_object::get_value(box, &value);
  }
  array_set(&value, a, index, value_type, CHECK);
JVM_END

JVM_ENTRY(jstring, JVM_GetClassName(JNIEnv *env, jclass cls))
  assert (cls != NULL, "illegal class");
  JVMWrapper("JVM_GetClassName");
  const char* name;
  if (java_lang_Class::is_primitive(JNIHandles::resolve(cls))) {
    name = type2name(java_lang_Class::primitive_type(JNIHandles::resolve(cls)));
  } else {
    // Consider caching interned string in Klass
    Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve(cls));
    assert(k->is_klass(), "just checking");
    name = k->external_name();
  }
  oop result = EnclaveMemory::vm_new_string(THREAD, (char*)name);
  return (jstring) JNIHandles::make_local(env, result);
JVM_END

JVM_ENTRY(jclass, JVM_GetComponentType(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_GetComponentType");
  oop mirror = JNIHandles::resolve_non_null(cls);
  if (java_lang_Class::is_primitive(mirror)) {
    return NULL;
  }

  Klass* klass = java_lang_Class::as_Klass(mirror);
  if (!klass->oop_is_array()) {
    return NULL;
  }

  oop result = ArrayKlass::cast(klass)->component_mirror();
  return (jclass) JNIHandles::make_local(env, result);
JVM_END

JVM_ENTRY(jboolean, JVM_IsArrayClass(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_IsArrayClass");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  return (k != NULL) && k->oop_is_array() ? JNI_TRUE : JNI_FALSE;
JVM_END

JVM_ENTRY(jboolean, JVM_IsInterface(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_IsInterface");
  oop mirror = JNIHandles::resolve_non_null(cls);
  if (java_lang_Class::is_primitive(mirror)) {
    return JNI_FALSE;
  }
  Klass* k = java_lang_Class::as_Klass(mirror);
  jboolean result = k->is_interface();
  assert(!result || k->oop_is_instance(),
         "all interfaces are instance types");
  // The compiler intrinsic for isInterface tests the
  // Klass::_access_flags bits in the same way.
  return result;
JVM_END

JVM_ENTRY(jboolean, JVM_IsPrimitiveClass(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_IsPrimitiveClass");
  oop mirror = JNIHandles::resolve_non_null(cls);
  return (jboolean) java_lang_Class::is_primitive(mirror);
JVM_END

JVM_ENTRY(jobjectArray, JVM_GetClassInterfaces(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_GetClassInterfaces");
  oop mirror = JNIHandles::resolve_non_null(cls);

  // Special handling for primitive objects
  if (java_lang_Class::is_primitive(mirror)) {
    // Primitive objects does not have any interfaces
    // TODO:
    return NULL;
  }

  KlassHandle klass(thread, java_lang_Class::as_Klass(mirror));
  // Figure size of result array
  int size;
  if (klass->oop_is_instance()) {
    size = InstanceKlass::cast(klass())->local_interfaces()->length();
  } else {
    assert(klass->oop_is_objArray() || klass->oop_is_typeArray(), "Illegal mirror klass");
    size = 2;
  }

  // Allocate result array
  objArrayOop r = (objArrayOop) EnclaveMemory::heapMemory->klass_new_array(THREAD,
                                 SystemDictionary::Class_klass()->java_mirror(), size);
  objArrayHandle result (THREAD, r);
  // Fill in result
  if (klass->oop_is_instance()) {
    // Regular instance klass, fill in all local interfaces
    for (int index = 0; index < size; index++) {
      Klass* k = InstanceKlass::cast(klass())->local_interfaces()->at(index);
      result->obj_at_put(index, k->java_mirror());
    }
  } else {
    // All arrays implement java.lang.Cloneable and java.io.Serializable
    result->obj_at_put(0, SystemDictionary::Cloneable_klass()->java_mirror());
    result->obj_at_put(1, SystemDictionary::Serializable_klass()->java_mirror());
  }
  return (jobjectArray) JNIHandles::make_local(env, result());
JVM_END

JVM_ENTRY(void, JVM_FillInStackTrace(JNIEnv *env, jobject receiver))
  JVMWrapper("JVM_FillInStackTrace");
JVM_END

JVM_ENTRY(jint, JVM_GetStackTraceDepth(JNIEnv *env, jobject throwable))
  JVMWrapper("JVM_GetStackTraceDepth");
  return 0;
JVM_END

JVM_ENTRY(jobject, JVM_GetStackTraceElement(JNIEnv *env, jobject throwable, jint index))
  JVMWrapper("JVM_GetStackTraceElement");
  return JNIHandles::make_local(env, NULL);
JVM_END

JVM_ENTRY(void , JVM_EnclaveDebug(JNIEnv* env, jclass ignore, jbyteArray array))
  JVMWrapper("JVM_EnclaveDebug");
  BREAKPOINT;
  unsigned char no = 0;
  jbyte* bytes = jni_functions()->GetByteArrayElements(env, array, &no);
  printf("%s", (char*)bytes);
  jni_functions()->ReleaseByteArrayElements(env, array, bytes, 0);
JVM_END

JVM_ENTRY(jobject , JVM_EnclaveCopy(JNIEnv* env, jclass ignore, jobject handle))
  JVMWrapper("JVM_EnclaveCopy");
  Handle obj(THREAD, JNIHandles::resolve_non_null(handle));
  Klass* klass = obj->klass();

  const int size = obj->size();
  oop new_obj_oop = NULL;
  bool default_copy_array_oop = true;
  bool copy_array_oop = false;

  if (obj->is_array()) {
      const int length = ((arrayOop)obj())->length();
      if (klass->oop_is_typeArray()) {
          new_obj_oop = (oop)EnclaveMemory::heapMemory->klass_type_array(THREAD, klass, length);
      } else {
          new_obj_oop = (oop)EnclaveMemory::heapMemory->klass_obj_array(THREAD, klass, length);
          // copy obj
          copy_array_oop = default_copy_array_oop;
      }
  } else {
      new_obj_oop = (oop)EnclaveMemory::heapMemory->vm_new_obj(THREAD, klass);
  }
  Copy::conjoint_jlongs_atomic((jlong*)obj(), (jlong*)new_obj_oop,
                               (size_t)align_object_size(size) / HeapWordsPerLong);

  if (copy_array_oop) {
      int length = arrayOop(obj())->length();
      oop coop;
      oop *arr_old = (oop*)((intptr_t) obj() + arrayOopDesc::base_offset_in_bytes(T_OBJECT));
      oop *arr_new = (oop*)((intptr_t) new_obj_oop + arrayOopDesc::base_offset_in_bytes(T_OBJECT));
      for (int i = 0;i < length;i++) {
          if (arr_old[i] != NULL) {
              // when it is array???
              coop = (oop)EnclaveMemory::heapMemory->vm_new_obj(THREAD, arr_old[i]->klass());
              Copy::conjoint_jlongs_atomic((jlong*)arr_old[i], (jlong*)coop,
                                           (size_t)align_object_size(arr_old[i]->size()) / HeapWordsPerLong);
              arr_new[i] = coop;
          }
      }
  }

  return JNIHandles::make_local(env, new_obj_oop);
JVM_END

JVM_ENTRY(jobject , JVM_EnclaveDeepCopy(JNIEnv* env, jclass ignore, jobject handle))
  JVMWrapper("JVM_EnclaveCopy");
  Handle obj(THREAD, JNIHandles::resolve_non_null(handle));
  oop obj_oop = obj();
  oop* oop_pointer = &obj_oop;
  EnclaveMemory::deep_copy(oop_pointer);
  return JNIHandles::make_local(env, *oop_pointer);
JVM_END

JVM_ENTRY(void , JVM_EnclaveClean(JNIEnv *env, jclass ignore))
  JVMWrapper("JVM_EnclaveClean");
  // clean other states
#ifdef ENCLAVE_UNIX
  EnclaveGC::reset();
#endif
JVM_END

JVM_ENTRY(jobject, JVM_CurrentThread(JNIEnv* env, jclass threadClass))
  JVMWrapper("JVM_CurrentThread");
  oop jthread = thread->threadObj();
//  assert (thread != NULL, "no current thread!");
  return JNIHandles::make_local(env, jthread);
JVM_END

JVM_ENTRY(jbyteArray , JVM_BytesEncrypt(JNIEnv* env, jclass ignore, jbyteArray array, jboolean en))
  JVMWrapper("JVM_BytesEncrypt");
  ResourceMark rm;
  size_t len = jni_functions()->GetArrayLength(env, array);
  if (len == 0) {
      return array;
  }
  jbyte* bytes = jni_functions()->GetByteArrayElements(env, array, false);
  jbyte* r_arr = (jbyte*)EnclaveCrypto::sgx_encrypt(THREAD, (char*)bytes, &len);
  EnclaveMemory* mem = (en)? EnclaveMemory::enclaveMemory : EnclaveMemory::heapMemory;
  jbyteArray result = (jbyteArray)JNIHandles::make_local(env, (oop)mem->vm_type_array(THREAD, T_BYTE, len));
  jni_functions()->SetByteArrayRegion(env, result, 0, len, r_arr);
  FreeHeap(bytes, mtInternal);
  return result;
JVM_END

JVM_ENTRY(jbyteArray , JVM_BytesDecrypt(JNIEnv* env, jclass ignore, jbyteArray array, jboolean en))
  JVMWrapper("JVM_BytesDecrypt");
  ResourceMark rm;
  size_t len = jni_functions()->GetArrayLength(env, array);
  jbyte* bytes = jni_functions()->GetByteArrayElements(env, array, false);
  jbyte* r_arr = (jbyte*)EnclaveCrypto::sgx_decrypt(THREAD, (char*)bytes, &len);
  EnclaveMemory* mem = (en)? EnclaveMemory::enclaveMemory : EnclaveMemory::heapMemory;
  jbyteArray result = (jbyteArray)JNIHandles::make_local(env, (oop)mem->vm_type_array(THREAD, T_BYTE, len));
  jni_functions()->SetByteArrayRegion(env, result, 0, len, r_arr);
  FreeHeap(bytes, mtInternal);
  return result;
JVM_END

JVM_ENTRY(jbyteArray , JVM_BytesEncryptInt(JNIEnv* env, jclass ignore, jintArray array, jboolean en))
  JVMWrapper("JVM_BytesEncryptInt");
  ResourceMark rm;
  size_t len = jni_functions()->GetArrayLength(env, array) * sizeof(jint);
  jint* bytes = jni_functions()->GetIntArrayElements(env, array, false);
  jbyte* r_arr = (jbyte*)EnclaveCrypto::sgx_encrypt(THREAD, (char*)bytes, &len);
  EnclaveMemory* mem = (en)? EnclaveMemory::enclaveMemory : EnclaveMemory::heapMemory;
  jbyteArray result = (jbyteArray)JNIHandles::make_local(env, (oop)mem->vm_type_array(THREAD, T_BYTE, len));
  jni_functions()->SetByteArrayRegion(env, result, 0, len, r_arr);
  FreeHeap(bytes, mtInternal);
  return result;
JVM_END

JVM_ENTRY(jintArray , JVM_BytesDecryptInt(JNIEnv* env, jclass ignore, jbyteArray array, jboolean en))
  JVMWrapper("JVM_BytesDecryptInt");
  ResourceMark rm;
  size_t len = jni_functions()->GetArrayLength(env, array);
  jbyte* bytes = jni_functions()->GetByteArrayElements(env, array, false);
  jint* r_arr = (jint*)EnclaveCrypto::sgx_decrypt(THREAD, (char*)bytes, &len);
  int int_len = len / sizeof(jint);
  EnclaveMemory* mem = (en)? EnclaveMemory::enclaveMemory : EnclaveMemory::heapMemory;
  jintArray result = (jintArray)JNIHandles::make_local(env, (oop)mem->vm_type_array(THREAD, T_INT, int_len));
  jni_functions()->SetIntArrayRegion(env, result, 0, len / sizeof(jint), r_arr);
  FreeHeap(bytes, mtInternal);
  return result;
JVM_END

JVM_ENTRY(jbyteArray , JVM_BytesEncryptDouble(JNIEnv* env, jclass ignore, jdoubleArray array, jboolean en))
  JVMWrapper("JVM_BytesEncryptDouble");
  ResourceMark rm;
  size_t len = jni_functions()->GetArrayLength(env, array) * sizeof(double);
  jdouble* bytes = jni_functions()->GetDoubleArrayElements(env, array, false);
  jbyte* r_arr = (jbyte*)EnclaveCrypto::sgx_encrypt(THREAD, (char*)bytes, &len);
  EnclaveMemory* mem = (en)? EnclaveMemory::enclaveMemory : EnclaveMemory::heapMemory;
  jbyteArray result = (jbyteArray)JNIHandles::make_local(env, (oop)mem->vm_type_array(THREAD, T_BYTE, len));
  jni_functions()->SetByteArrayRegion(env, result, 0, len, r_arr);
  FreeHeap(bytes, mtInternal);
  return result;
JVM_END

JVM_ENTRY(jdoubleArray , JVM_BytesDecryptDouble(JNIEnv* env, jclass ignore, jbyteArray array, jboolean en))
  JVMWrapper("JVM_BytesDecrypt");
  ResourceMark rm;
  size_t len = jni_functions()->GetArrayLength(env, array);
  jbyte* bytes = jni_functions()->GetByteArrayElements(env, array, false);
  jdouble* r_arr = (jdouble*)EnclaveCrypto::sgx_decrypt(THREAD, (char*)bytes, &len);
  EnclaveMemory* mem = (en)? EnclaveMemory::enclaveMemory : EnclaveMemory::heapMemory;
  jdoubleArray result = (jdoubleArray)JNIHandles::make_local(env, (oop)mem->vm_type_array(THREAD, T_DOUBLE, len / sizeof(double)));
  jni_functions()->SetDoubleArrayRegion(env, result, 0, len / sizeof(double), r_arr);
  FreeHeap(bytes, mtInternal);
  return result;
JVM_END

JVM_ENTRY(jbyteArray , JVM_BytesHash(JNIEnv* env, jclass ignore, jbyteArray array, jboolean en))
  JVMWrapper("JVM_BytesDecrypt");
  ResourceMark rm;
  size_t len = jni_functions()->GetArrayLength(env, array);
  jbyte* bytes = jni_functions()->GetByteArrayElements(env, array, false);
  jbyte* hash = (jbyte*)EnclaveCrypto::sgx_hash(THREAD, (char*)bytes, len);
  EnclaveMemory* mem = (en)? EnclaveMemory::enclaveMemory : EnclaveMemory::heapMemory;
  jbyteArray result = (jbyteArray)JNIHandles::make_local(env, (oop)mem->vm_type_array(THREAD, T_BYTE, ENCLAVE_HASH_SIZE));
  jni_functions()->SetByteArrayRegion(env, result, 0, ENCLAVE_HASH_SIZE, hash);
  FreeHeap(bytes, mtInternal);
  return result;
JVM_END

JVM_ENTRY(jboolean , JVM_BytesHashVerify(JNIEnv* env, jclass ignore, jbyteArray array, jbyteArray hash_arr))
  JVMWrapper("JVM_BytesDecrypt");
  ResourceMark rm;
  size_t len = jni_functions()->GetArrayLength(env, array);
  jbyte* bytes = jni_functions()->GetByteArrayElements(env, array, false);
  jbyte* hash = jni_functions()->GetByteArrayElements(env, hash_arr, false);
  bool verified = EnclaveCrypto::sgx_verify(THREAD, (char*)bytes, len, (char*)hash);
  FreeHeap(bytes, mtInternal);
  FreeHeap(hash, mtInternal);
  return (verified)? JNI_TRUE : JNI_FALSE;
JVM_END