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
#include "classfile/sharedClassUtil.hpp"
#include "classfile/systemDictionaryShared.hpp"
#endif
#include "classfile/vmSymbols.hpp"
//#include "gc_interface/collectedHeap.inline.hpp"
#include "interpreter/bytecode.hpp"
#include "memory/oopFactory.hpp"
#include "memory/universe.inline.hpp"
#include "oops/fieldStreams.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/method.hpp"
#include "prims/jvm.h"
#include "prims/jvm_misc.hpp"
#include "prims/nativeLookup.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/java.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/jfieldIDWorkaround.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "runtime/os.hpp"
#include "runtime/reflection.hpp"
#include "runtime/vframe.hpp"
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

#include <errno.h>

#define err_msg(a, b) a

/*
  NOTE about use of any ctor or function call that can trigger a safepoint/GC:
  such ctors and calls MUST NOT come between an oop declaration/init and its
  usage because if objects are move this may cause various memory stomps, bus
  errors and segfaults. Here is a cookbook for causing so called "naked oop
  failures":

      JVM_ENTRY(jobjectArray, JVM_GetClassDeclaredFields<etc> {
          JVMWrapper("JVM_GetClassDeclaredFields");

          // Object address to be held directly in mirror & not visible to GC
          oop mirror = JNIHandles::resolve_non_null(ofClass);

          // If this ctor can hit a safepoint, moving objects around, then
          ComplexConstructor foo;

          // Boom! mirror may point to JUNK instead of the intended object
          (some dereference of mirror)

          // Here's another call that may block for GC, making mirror stale
          MutexLocker ml(some_lock);

          // And here's an initializer that can result in a stale oop
          // all in one step.
          oop o = call_that_can_throw_exception(TRAPS);


  The solution is to keep the oop declaration BELOW the ctor or function
  call that might cause a GC, do another resolve to reassign the oop, or
  consider use of a Handle instead of an oop so there is immunity from object
  motion. But note that the "QUICK" entries below do not have a handlemark
  and thus can only support use of handles passed in.
*/


static void trace_class_resolution_impl(Klass* to_class, TRAPS) {
  ResourceMark rm;
  int line_number = -1;
  const char * source_file = NULL;
  const char * trace = "explicit";
  InstanceKlass* caller = NULL;
  JavaThread* jthread = JavaThread::current();
  if (jthread->has_last_Java_frame()) {
    vframeStream vfst(jthread);

    // scan up the stack skipping ClassLoader, AccessController and PrivilegedAction frames
    TempNewSymbol access_controller = SymbolTable::new_symbol("java/security/AccessController", CHECK);
    Klass* access_controller_klass = SystemDictionary::resolve_or_fail(access_controller, false, CHECK);
    TempNewSymbol privileged_action = SymbolTable::new_symbol("java/security/PrivilegedAction", CHECK);
    Klass* privileged_action_klass = SystemDictionary::resolve_or_fail(privileged_action, false, CHECK);

    Method* last_caller = NULL;

    while (!vfst.at_end()) {
      Method* m = vfst.method();
      if (!vfst.method()->method_holder()->is_subclass_of(SystemDictionary::ClassLoader_klass())&&
          !vfst.method()->method_holder()->is_subclass_of(access_controller_klass) &&
          !vfst.method()->method_holder()->is_subclass_of(privileged_action_klass)) {
        break;
      }
      last_caller = m;
      vfst.next();
    }
    // if this is called from Class.forName0 and that is called from Class.forName,
    // then print the caller of Class.forName.  If this is Class.loadClass, then print
    // that caller, otherwise keep quiet since this should be picked up elsewhere.
    bool found_it = false;
    if (!vfst.at_end() &&
        vfst.method()->method_holder()->name() == vmSymbols::java_lang_Class() &&
        vfst.method()->name() == vmSymbols::forName0_name()) {
      vfst.next();
      if (!vfst.at_end() &&
          vfst.method()->method_holder()->name() == vmSymbols::java_lang_Class() &&
          vfst.method()->name() == vmSymbols::forName_name()) {
        vfst.next();
        found_it = true;
      }
    } else if (last_caller != NULL &&
               last_caller->method_holder()->name() ==
               vmSymbols::java_lang_ClassLoader() &&
               (last_caller->name() == vmSymbols::loadClassInternal_name() ||
                last_caller->name() == vmSymbols::loadClass_name())) {
      found_it = true;
    } else if (!vfst.at_end()) {
      if (vfst.method()->is_native()) {
        // JNI call
        found_it = true;
      }
    }
    if (found_it && !vfst.at_end()) {
      // found the caller
      caller = vfst.method()->method_holder();
      line_number = vfst.method()->line_number_from_bci(vfst.bci());
      if (line_number == -1) {
        // show method name if it's a native method
        trace = vfst.method()->name_and_sig_as_C_string();
      }
      Symbol* s = caller->source_file_name();
      if (s != NULL) {
        source_file = s->as_C_string();
      }
    }
  }
  if (caller != NULL) {
    if (to_class != caller) {
      const char * from = caller->external_name();
      const char * to = to_class->external_name();
      // print in a single call to reduce interleaving between threads
      if (source_file != NULL) {
        tty->print("RESOLVE %s %s %s:%d (%s)\n", from, to, source_file, line_number, trace);
      } else {
        tty->print("RESOLVE %s %s (%s)\n", from, to, trace);
      }
    }
  }
}

void trace_class_resolution(Klass* to_class) {
  EXCEPTION_MARK;
  trace_class_resolution_impl(to_class, THREAD);
  if (HAS_PENDING_EXCEPTION) {
    CLEAR_PENDING_EXCEPTION;
  }
}

// Wrapper to trace JVM functions

#ifdef ASSERT
class JVMTraceWrapper : public StackObj {
   public:
    JVMTraceWrapper(const char* format, ...) ATTRIBUTE_PRINTF(2, 3) {
      if (TraceJVMCalls) {
        va_list ap;
        va_start(ap, format);
        tty->print("JVM ");
        tty->vprint_cr(format, ap);
        va_end(ap);
      }
    }
  };

  Histogram* JVMHistogram;
  volatile jint JVMHistogram_lock = 0;

  class JVMHistogramElement : public HistogramElement {
    public:
     JVMHistogramElement(const char* name);
  };

  JVMHistogramElement::JVMHistogramElement(const char* elementName) {
    _name = elementName;
    uintx count = 0;

    while (Atomic::cmpxchg(1, &JVMHistogram_lock, 0) != 0) {
      while (OrderAccess::load_acquire(&JVMHistogram_lock) != 0) {
        count +=1;
        if ( (WarnOnStalledSpinLock > 0)
          && (count % WarnOnStalledSpinLock == 0)) {
          warning("JVMHistogram_lock seems to be stalled");
        }
      }
     }

    if(JVMHistogram == NULL)
      JVMHistogram = new Histogram("JVM Call Counts",100);

    JVMHistogram->add_element(this);
    Atomic::dec(&JVMHistogram_lock);
  }

#define JVMCountWrapper(arg) \
      static JVMHistogramElement* e = new JVMHistogramElement(arg); \
      if (e != NULL) e->increment_count();  // Due to bug in VC++, we need a NULL check here eventhough it should never happen!

#define JVMWrapper(arg1)                    JVMCountWrapper(arg1); JVMTraceWrapper(arg1)
#define JVMWrapper2(arg1, arg2)             JVMCountWrapper(arg1); JVMTraceWrapper(arg1, arg2)
#define JVMWrapper3(arg1, arg2, arg3)       JVMCountWrapper(arg1); JVMTraceWrapper(arg1, arg2, arg3)
#define JVMWrapper4(arg1, arg2, arg3, arg4) JVMCountWrapper(arg1); JVMTraceWrapper(arg1, arg2, arg3, arg4)
#else
#define JVMWrapper(arg1)
#define JVMWrapper2(arg1, arg2)
#define JVMWrapper3(arg1, arg2, arg3)
#define JVMWrapper4(arg1, arg2, arg3, arg4)
#endif


// Interface version /////////////////////////////////////////////////////////////////////


JVM_LEAF(jint, JVM_GetInterfaceVersion())
  return JVM_INTERFACE_VERSION;
JVM_END


// java.lang.System //////////////////////////////////////////////////////////////////////


JVM_LEAF(jlong, JVM_CurrentTimeMillis(JNIEnv *env, jclass ignored))
  JVMWrapper("JVM_CurrentTimeMillis");
  return os::javaTimeMillis();
JVM_END

JVM_LEAF(jlong, JVM_NanoTime(JNIEnv *env, jclass ignored))
  JVMWrapper("JVM_NanoTime");
  return os::javaTimeNanos();
JVM_END


JVM_ENTRY(void, JVM_ArrayCopy(JNIEnv *env, jclass ignored, jobject src, jint src_pos,
        jobject dst, jint dst_pos, jint length))
  JVMWrapper("JVM_ArrayCopy");
  // Check if we have null pointers
#ifdef ENCLAVE_UNIX
  if (src == NULL || dst == NULL) {
    THROW(vmSymbols::java_lang_NullPointerException());
  }
  arrayOop s = arrayOop(JNIHandles::resolve_non_null(src));
  arrayOop d = arrayOop(JNIHandles::resolve_non_null(dst));
  EnclaveMemory::copy_array(s, src_pos, d, dst_pos, length);
#else
  if (src == NULL || dst == NULL) {
    THROW(vmSymbols::java_lang_NullPointerException());
  }
  arrayOop s = arrayOop(JNIHandles::resolve_non_null(src));
  arrayOop d = arrayOop(JNIHandles::resolve_non_null(dst));
  assert(s->is_oop(), "JVM_ArrayCopy: src not an oop");
  assert(d->is_oop(), "JVM_ArrayCopy: dst not an oop");
  // Do copy
  s->klass()->copy_array(s, src_pos, d, dst_pos, length, thread);
#endif
JVM_END


static void set_property(Handle props, const char* key, const char* value, TRAPS) {
  JavaValue r(T_OBJECT);
  // public synchronized Object put(Object key, Object value);
  HandleMark hm(THREAD);
  Handle key_str    = java_lang_String::create_from_platform_dependent_str(key, CHECK);
  Handle value_str  = java_lang_String::create_from_platform_dependent_str((value != NULL ? value : ""), CHECK);
  JavaCalls::call_virtual(&r,
                          props,
                          KlassHandle(THREAD, SystemDictionary::Properties_klass()),
                          vmSymbols::put_name(),
                          vmSymbols::object_object_object_signature(),
                          key_str,
                          value_str,
                          THREAD);
}


#define PUTPROP(props, name, value) set_property((props), (name), (value), CHECK_(properties));


JVM_ENTRY(jobject, JVM_InitProperties(JNIEnv *env, jobject properties))
  JVMWrapper("JVM_InitProperties");
  D_WARN_Unimplement;
JVM_END


/*
 * Return the temporary directory that the VM uses for the attach
 * and perf data files.
 *
 * It is important that this directory is well-known and the
 * same for all VM instances. It cannot be affected by configuration
 * variables such as java.io.tmpdir.
 */
JVM_ENTRY(jstring, JVM_GetTemporaryDirectory(JNIEnv *env))
  JVMWrapper("JVM_GetTemporaryDirectory");
  HandleMark hm(THREAD);
  const char* temp_dir = os::get_temp_directory();
  Handle h = java_lang_String::create_from_platform_dependent_str(temp_dir, CHECK_NULL);
  return (jstring) JNIHandles::make_local(env, h());
JVM_END


// java.lang.Runtime /////////////////////////////////////////////////////////////////////////

extern volatile jint vm_created;

JVM_ENTRY_NO_ENV(void, JVM_Exit(jint code))
  D_WARN_Unimplement;
JVM_END


JVM_ENTRY_NO_ENV(void, JVM_Halt(jint code))
  D_WARN_Unimplement;
JVM_END


JVM_LEAF(void, JVM_OnExit(void (*func)(void)))
  D_WARN_Unimplement;
JVM_END


JVM_ENTRY_NO_ENV(void, JVM_GC(void))
  JVMWrapper("JVM_GC");
  D_WARN_Unimplement;
JVM_END


JVM_LEAF(jlong, JVM_MaxObjectInspectionAge(void))
  JVMWrapper("JVM_MaxObjectInspectionAge");
  D_WARN_Unimplement;
JVM_END


JVM_LEAF(void, JVM_TraceInstructions(jboolean on))
  if (PrintJVMWarnings) warning("JVM_TraceInstructions not supported");
JVM_END


JVM_LEAF(void, JVM_TraceMethodCalls(jboolean on))
  if (PrintJVMWarnings) warning("JVM_TraceMethodCalls not supported");
JVM_END

static inline jlong convert_size_t_to_jlong(size_t val) {
  // In the 64-bit vm, a size_t can overflow a jlong (which is signed).
  NOT_LP64 (return (jlong)val;)
  LP64_ONLY(return (jlong)MIN2(val, (size_t)max_jlong);)
}

JVM_ENTRY_NO_ENV(jlong, JVM_TotalMemory(void))
  JVMWrapper("JVM_TotalMemory");
  D_WARN_Unimplement;
JVM_END


JVM_ENTRY_NO_ENV(jlong, JVM_FreeMemory(void))
  JVMWrapper("JVM_FreeMemory");
  D_WARN_Unimplement;
JVM_END


JVM_ENTRY_NO_ENV(jlong, JVM_MaxMemory(void))
  JVMWrapper("JVM_MaxMemory");
  D_WARN_Unimplement;
JVM_END


JVM_ENTRY_NO_ENV(jint, JVM_ActiveProcessorCount(void))
  JVMWrapper("JVM_ActiveProcessorCount");
  return os::active_processor_count();
JVM_END



// java.lang.Throwable //////////////////////////////////////////////////////


JVM_ENTRY(void, JVM_FillInStackTrace(JNIEnv *env, jobject receiver))
  JVMWrapper("JVM_FillInStackTrace");
#ifndef ENCLAVE_UNIX
  Handle exception(thread, JNIHandles::resolve_non_null(receiver));
  java_lang_Throwable::fill_in_stack_trace(exception);
#endif
JVM_END


JVM_ENTRY(jint, JVM_GetStackTraceDepth(JNIEnv *env, jobject throwable))
  JVMWrapper("JVM_GetStackTraceDepth");
  oop exception = JNIHandles::resolve(throwable);
  return java_lang_Throwable::get_stack_trace_depth(exception, THREAD);
JVM_END


JVM_ENTRY(jobject, JVM_GetStackTraceElement(JNIEnv *env, jobject throwable, jint index))
  JVMWrapper("JVM_GetStackTraceElement");
  // JvmtiVMObjectAllocEventCollector oam; // This ctor (throughout this module) may trigger a safepoint/GC
  oop exception = JNIHandles::resolve(throwable);
  oop element = java_lang_Throwable::get_stack_trace_element(exception, index, CHECK_NULL);
  return JNIHandles::make_local(env, element);
JVM_END

// enclave api
JVM_ENTRY(jbyteArray , JVM_BytesEncrypt(JNIEnv* env, jclass ignore, jbyteArray array, jboolean en))
  JVMWrapper("JVM_BytesEncrypt");
  size_t len = jni_functions()->GetArrayLength(env, array);
  if (len == 0) {
    return array;
  }
  jbyte* bytes = jni_functions()->GetByteArrayElements(env, array, false);
  jbyte* r_arr = EnclaveCrypto::sgx_encrypt(THREAD, bytes, &len);
  EnclaveMemory* mem = (en)? EnclaveMemory::enclaveMemory : EnclaveMemory::heapMemory;
  jbyteArray result = (jbyteArray)JNIHandles::make_local(env, (oop)mem->vm_type_array(THREAD, T_BYTE, len));
  jni_functions()->SetByteArrayRegion(env, result, 0, len, r_arr);
  return result;
JVM_END

JVM_ENTRY(jbyteArray , JVM_BytesDecrypt(JNIEnv* env, jclass ignore, jbyteArray array, jboolean en))
  JVMWrapper("JVM_BytesDecrypt");
  size_t len = jni_functions()->GetArrayLength(env, array);
  jbyte* bytes = jni_functions()->GetByteArrayElements(env, array, false);
  jbyte* r_arr = EnclaveCrypto::sgx_decrypt((JavaThread*)THREAD, bytes, &len);
  EnclaveMemory* mem = (en)? EnclaveMemory::enclaveMemory : EnclaveMemory::heapMemory;
  jbyteArray result = (jbyteArray)JNIHandles::make_local(env, (oop)mem->vm_type_array(THREAD, T_BYTE, len));
  jni_functions()->SetByteArrayRegion(env, result, 0, len, r_arr);
  return result;
JVM_END

JVM_ENTRY(jbyteArray , JVM_BytesEncryptInt(JNIEnv* env, jclass ignore, jintArray array, jboolean en))
  JVMWrapper("JVM_BytesEncryptInt");
  size_t len = jni_functions()->GetArrayLength(env, array) * sizeof(jint);
  jint* bytes = jni_functions()->GetIntArrayElements(env, array, false);
  jbyte* r_arr = EnclaveCrypto::sgx_encrypt(THREAD, (jbyte*)bytes, &len);
  EnclaveMemory* mem = (en)? EnclaveMemory::enclaveMemory : EnclaveMemory::heapMemory;
  jbyteArray result = (jbyteArray)JNIHandles::make_local(env, (oop)mem->vm_type_array(THREAD, T_BYTE, len));
  jni_functions()->SetByteArrayRegion(env, result, 0, len, r_arr);
  return result;
JVM_END

JVM_ENTRY(jintArray , JVM_BytesDecryptInt(JNIEnv* env, jclass ignore, jbyteArray array, jboolean en))
  JVMWrapper("JVM_BytesDecryptInt");
#ifdef ENCLAVE_UNIX
  size_t len = jni_functions()->GetArrayLength(env, array);
  jbyte* bytes = jni_functions()->GetByteArrayElements(env, array, false);
  jint* r_arr = (jint*)EnclaveCrypto::sgx_decrypt((JavaThread*)THREAD, bytes, &len);
  int int_len = len / sizeof(jint);
  EnclaveMemory* mem = (en)? EnclaveMemory::enclaveMemory : EnclaveMemory::heapMemory;
  jintArray result = (jintArray)JNIHandles::make_local(env, (oop)mem->vm_type_array(THREAD, T_INT, int_len));
  jni_functions()->SetIntArrayRegion(env, result, 0, len / sizeof(jint), int_len);
  return result;
#else
  return NULL;
#endif
JVM_END

JVM_ENTRY(jbyteArray , JVM_BytesEncryptDouble(JNIEnv* env, jclass ignore, jdoubleArray array, jboolean en))
  JVMWrapper("JVM_BytesEncryptDouble");
#ifdef ENCLAVE_UNIX
  size_t len = jni_functions()->GetArrayLength(env, array) * sizeof(double);
  jdouble* bytes = jni_functions()->GetDoubleArrayElements(env, array, false);
  jbyte* r_arr = EnclaveCrypto::sgx_encrypt(THREAD, (jbyte*)bytes, &len);
  EnclaveMemory* mem = (en)? EnclaveMemory::enclaveMemory : EnclaveMemory::heapMemory;
  jbyteArray result = (jbyteArray)JNIHandles::make_local(env, (oop)mem->vm_type_array(THREAD, T_BYTE, len));
  jni_functions()->SetByteArrayRegion(env, result, 0, len, r_arr);
  return result;
#else
  return NULL;
#endif
JVM_END

JVM_ENTRY(jdoubleArray , JVM_BytesDecryptDouble(JNIEnv* env, jclass ignore, jbyteArray array, jboolean en))
  JVMWrapper("JVM_BytesDecrypt");
  size_t len = jni_functions()->GetArrayLength(env, array);
  jbyte* bytes = jni_functions()->GetByteArrayElements(env, array, false);
  jdouble* r_arr = (jdouble*)EnclaveCrypto::sgx_decrypt((JavaThread*)THREAD, bytes, &len);
  EnclaveMemory* mem = (en)? EnclaveMemory::enclaveMemory : EnclaveMemory::heapMemory;
  jdoubleArray result = (jdoubleArray)JNIHandles::make_local(env, (oop)mem->vm_type_array(THREAD, T_DOUBLE, len / sizeof(double)));
  jni_functions()->SetDoubleArrayRegion(env, result, 0, len / sizeof(double), r_arr);
  return result;
JVM_END

JVM_ENTRY(jbyteArray , JVM_BytesHash(JNIEnv* env, jclass ignore, jbyteArray array, jboolean en))
  JVMWrapper("JVM_BytesDecrypt");
  size_t len = jni_functions()->GetArrayLength(env, array);
  jbyte* bytes = jni_functions()->GetByteArrayElements(env, array, false);
  jbyte* hash = EnclaveCrypto::sgx_hash(THREAD, (char*)bytes, len);
  EnclaveMemory* mem = (en)? EnclaveMemory::enclaveMemory : EnclaveMemory::heapMemory;
  jbyteArray result = (jbyteArray)JNIHandles::make_local(env, (oop)mem->vm_type_array(THREAD, T_BYTE, ENCLAVE_HASH_SIZE));
  jni_functions()->SetByteArrayRegion(env, result, 0, ENCLAVE_HASH_SIZE, hash);
  return result;
JVM_END

JVM_ENTRY(jboolean , JVM_BytesHashVerify(JNIEnv* env, jclass ignore, jbyteArray array, jbyteArray hash_arr))
  JVMWrapper("JVM_BytesDecrypt");
#ifdef ENCLAVE_UNIX
  size_t len = jni_functions()->GetArrayLength(env, array);
  jbyte* bytes = jni_functions()->GetByteArrayElements(env, array, false);
  jbyte* hash = jni_functions()->GetByteArrayElements(env, hash_arr, false);
  bool verified = EnclaveCrypto::sgx_verify(THREAD, (char*)bytes, len, (char*)hash);
  return (verified)? JNI_TRUE : JNI_FALSE;
#else
  return JNI_FALSE;
#endif
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
#ifdef ENCLAVE_UNIX
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
#else
  return handle;
#endif

JVM_END

JVM_ENTRY(jobject , JVM_EnclaveDeepCopy(JNIEnv* env, jclass ignore, jobject handle))
  JVMWrapper("JVM_EnclaveCopy");
#ifdef ENCLAVE_UNIX
  Handle obj(THREAD, JNIHandles::resolve_non_null(handle));
  oop obj_oop = obj();
  oop* oop_pointer = &obj_oop;
  EnclaveMemory::deep_copy(oop_pointer);
  return JNIHandles::make_local(env, *oop_pointer);
#else
  return handle;
#endif

JVM_END

JVM_ENTRY(void , JVM_EnclaveClean(JNIEnv *env, jclass ignore))
  JVMWrapper("JVM_EnclaveClean");
  // clean other states
#ifdef ENCLAVE_UNIX
  EnclaveGC::reset();
#endif
JVM_END

// java.lang.Object ///////////////////////////////////////////////


JVM_ENTRY(jint, JVM_IHashCode(JNIEnv* env, jobject handle))
  JVMWrapper("JVM_IHashCode");
  // as implemented in the classic virtual machine; return 0 if object is NULL
  return handle == NULL ? 0 : ObjectSynchronizer::FastHashCode (THREAD, JNIHandles::resolve_non_null(handle)) ;
JVM_END


JVM_ENTRY(void, JVM_MonitorWait(JNIEnv* env, jobject handle, jlong ms))
  JVMWrapper("JVM_MonitorWait");
  Handle obj(THREAD, JNIHandles::resolve_non_null(handle));
  ObjectSynchronizer::wait(obj, ms, CHECK);
JVM_END


JVM_ENTRY(void, JVM_MonitorNotify(JNIEnv* env, jobject handle))
  JVMWrapper("JVM_MonitorNotify");
  Handle obj(THREAD, JNIHandles::resolve_non_null(handle));
  ObjectSynchronizer::notify(obj, CHECK);
JVM_END


JVM_ENTRY(void, JVM_MonitorNotifyAll(JNIEnv* env, jobject handle))
  JVMWrapper("JVM_MonitorNotifyAll");
  Handle obj(THREAD, JNIHandles::resolve_non_null(handle));
  ObjectSynchronizer::notifyall(obj, CHECK);
JVM_END


JVM_ENTRY(jobject, JVM_Clone(JNIEnv* env, jobject handle))
  JVMWrapper("JVM_Clone");

#ifdef ENCLAVE_UNIX
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
#else

  Handle obj(THREAD, JNIHandles::resolve_non_null(handle));
  const KlassHandle klass (THREAD, obj->klass());
  // JvmtiVMObjectAllocEventCollector oam;

#ifdef ASSERT
  // Just checking that the cloneable flag is set correct
  if (obj->is_array()) {
    guarantee(klass->is_cloneable(), "all arrays are cloneable");
  } else {
    guarantee(obj->is_instance(), "should be instanceOop");
    bool cloneable = klass->is_subtype_of(SystemDictionary::Cloneable_klass());
    guarantee(cloneable == klass->is_cloneable(), "incorrect cloneable flag");
  }
#endif

  // Check if class of obj supports the Cloneable interface.
  // All arrays are considered to be cloneable (See JLS 20.1.5)
  if (!klass->is_cloneable()) {
    ResourceMark rm(THREAD);
    THROW_MSG_0(vmSymbols::java_lang_CloneNotSupportedException(), klass->external_name());
  }

  // Make shallow object copy
  const int size = obj->size();
  oop new_obj_oop = NULL;
  if (obj->is_array()) {
    const int length = ((arrayOop)obj())->length();
    new_obj_oop = CollectedHeap::array_allocate(klass, size, length, CHECK_NULL);
  } else {
    new_obj_oop = CollectedHeap::obj_allocate(klass, size, CHECK_NULL);
  }

  // 4839641 (4840070): We must do an oop-atomic copy, because if another thread
  // is modifying a reference field in the clonee, a non-oop-atomic copy might
  // be suspended in the middle of copying the pointer and end up with parts
  // of two different pointers in the field.  Subsequent dereferences will crash.
  // 4846409: an oop-copy of objects with long or double fields or arrays of same
  // won't copy the longs/doubles atomically in 32-bit vm's, so we copy jlongs instead
  // of oops.  We know objects are aligned on a minimum of an jlong boundary.
  // The same is true of StubRoutines::object_copy and the various oop_copy
  // variants, and of the code generated by the inline_native_clone intrinsic.
  assert(MinObjAlignmentInBytes >= BytesPerLong, "objects misaligned");
  Copy::conjoint_jlongs_atomic((jlong*)obj(), (jlong*)new_obj_oop,
                               (size_t)align_object_size(size) / HeapWordsPerLong);
  // Clear the header
  new_obj_oop->init_mark();

  // Store check (mark entire object and let gc sort it out)
  BarrierSet* bs = Universe::heap()->barrier_set();
  assert(bs->has_write_region_opt(), "Barrier set does not have write_region");
  bs->write_region(MemRegion((HeapWord*)new_obj_oop, size));

  Handle new_obj(THREAD, new_obj_oop);
  // Special handling for MemberNames.  Since they contain Method* metadata, they
  // must be registered so that RedefineClasses can fix metadata contained in them.
  if (java_lang_invoke_MemberName::is_instance(new_obj()) &&
      java_lang_invoke_MemberName::is_method(new_obj())) {
    Method* method = (Method*)java_lang_invoke_MemberName::vmtarget(new_obj());
    // MemberName may be unresolved, so doesn't need registration until resolved.
    if (method != NULL) {
      methodHandle m(THREAD, method);
      // This can safepoint and redefine method, so need both new_obj and method
      // in a handle, for two different reasons.  new_obj can move, method can be
      // deleted if nothing is using it on the stack.
      m->method_holder()->add_member_name(new_obj(), false);
    }
  }

  // Caution: this involves a java upcall, so the clone should be
  // "gc-robust" by this stage.
  if (klass->has_finalizer()) {
    assert(obj->is_instance(), "should be instanceOop");
    new_obj_oop = InstanceKlass::register_finalizer(instanceOop(new_obj()), CHECK_NULL);
    new_obj = Handle(THREAD, new_obj_oop);
  }

  return JNIHandles::make_local(env, new_obj());
#endif
JVM_END

// java.lang.Compiler ////////////////////////////////////////////////////

// The initial cuts of the HotSpot VM will not support JITs, and all existing
// JITs would need extensive changes to work with HotSpot.  The JIT-related JVM
// functions are all silently ignored unless JVM warnings are printed.

JVM_LEAF(void, JVM_InitializeCompiler (JNIEnv *env, jclass compCls))
  if (PrintJVMWarnings) warning("JVM_InitializeCompiler not supported");
JVM_END


JVM_LEAF(jboolean, JVM_IsSilentCompiler(JNIEnv *env, jclass compCls))
  if (PrintJVMWarnings) warning("JVM_IsSilentCompiler not supported");
  return JNI_FALSE;
JVM_END


JVM_LEAF(jboolean, JVM_CompileClass(JNIEnv *env, jclass compCls, jclass cls))
  if (PrintJVMWarnings) warning("JVM_CompileClass not supported");
  return JNI_FALSE;
JVM_END


JVM_LEAF(jboolean, JVM_CompileClasses(JNIEnv *env, jclass cls, jstring jname))
  if (PrintJVMWarnings) warning("JVM_CompileClasses not supported");
  return JNI_FALSE;
JVM_END


JVM_LEAF(jobject, JVM_CompilerCommand(JNIEnv *env, jclass compCls, jobject arg))
  if (PrintJVMWarnings) warning("JVM_CompilerCommand not supported");
  return NULL;
JVM_END


JVM_LEAF(void, JVM_EnableCompiler(JNIEnv *env, jclass compCls))
  if (PrintJVMWarnings) warning("JVM_EnableCompiler not supported");
JVM_END


JVM_LEAF(void, JVM_DisableCompiler(JNIEnv *env, jclass compCls))
  if (PrintJVMWarnings) warning("JVM_DisableCompiler not supported");
JVM_END



// Error message support //////////////////////////////////////////////////////

JVM_LEAF(jint, JVM_GetLastErrorString(char *buf, int len))
  JVMWrapper("JVM_GetLastErrorString");
  return (jint)os::lasterror(buf, len);
JVM_END


// java.io.File ///////////////////////////////////////////////////////////////

JVM_LEAF(char*, JVM_NativePath(char* path))
  JVMWrapper2("JVM_NativePath (%s)", path);
  return os::native_path(path);
JVM_END

JVM_LEAF(void, JVM_CopySwapMemory(JNIEnv *env, jobject srcObj, jlong srcOffset,
        jobject dstObj, jlong dstOffset, jlong size,
        jlong elemSize)) {
    D_WARN_Unimplement;
  } JVM_END

// Misc. class handling ///////////////////////////////////////////////////////////
JVM_ENTRY(jclass, JVM_GetCallerClass(JNIEnv* env, int depth))
  JVMWrapper("JVM_GetCallerClass");
  // Pre-JDK 8 and early builds of JDK 8 don't have a CallerSensitive annotation; or
  // sun.reflect.Reflection.getCallerClass with a depth parameter is provided
  // temporarily for existing code to use until a replacement API is defined.
//  if (SystemDictionary::reflect_CallerSensitive_klass() == NULL || depth != JVM_CALLER_DEPTH) {
//    D_WARN_Unimplement;
//    return NULL;
//  }

  // Getting the class of the caller frame.
  //
  // The call stack at this point looks something like this:
  //
  // [0] [ @CallerSensitive public sun.reflect.Reflection.getCallerClass ]
  // [1] [ @CallerSensitive API.method                                   ]
  // [.] [ (skipped intermediate frames)                                 ]
  // [n] [ caller                                                        ]
  vframeStream vfst(thread);
  // Cf. LibraryCallKit::inline_native_Reflection_getCallerClass
  for (int n = 0; !vfst.at_end(); vfst.security_next(), n++) {
    Method* m = vfst.method();
    assert(m != NULL, "sanity");
    switch (n) {
      case 0:
        // This must only be called from Reflection.getCallerClass
        if (m->intrinsic_id() != vmIntrinsics::_getCallerClass) {
          THROW_MSG_NULL(vmSymbols::java_lang_InternalError(), "JVM_GetCallerClass must only be called from Reflection.getCallerClass");
        }
        // fall-through
      case 1:
        // Frame 0 and 1 must be caller sensitive.
        if (!m->caller_sensitive()) {
          THROW_MSG_NULL(vmSymbols::java_lang_InternalError(), err_msg("CallerSensitive annotation expected at frame %d", n));
        }
        break;
      default:
        if (!m->is_ignored_by_security_stack_walk()) {
          // We have reached the desired frame; return the holder class.
          return (jclass) JNIHandles::make_local(env, m->method_holder()->java_mirror());
        }
        break;
    }
  }
  RegisterMap map(JavaThread::current());
  frame frame_1 = JavaThread::current()->last_frame();
  frame frame_2 = frame_1.sender_for_interpreter_frame(&map);
  frame target_frame = frame_2.sender_for_interpreter_frame(&map);
  Klass *k = target_frame.interpreter_frame_method()->method_holder();
  return (k == NULL) ? NULL : (jclass) JNIHandles::make_local(env, k->java_mirror());
JVM_END


JVM_ENTRY(jclass, JVM_FindPrimitiveClass(JNIEnv* env, const char* utf))
  JVMWrapper("JVM_FindPrimitiveClass");
  oop mirror = NULL;
  BasicType t = name2type(utf);
  if (t != T_ILLEGAL && t != T_OBJECT && t != T_ARRAY) {
    mirror = Universe::java_mirror(t);
  }
  if (mirror == NULL) {
    THROW_MSG_0(vmSymbols::java_lang_ClassNotFoundException(), (char*) utf);
  } else {
    return (jclass) JNIHandles::make_local(env, mirror);
  }
JVM_END


JVM_ENTRY(void, JVM_ResolveClass(JNIEnv* env, jclass cls))
  JVMWrapper("JVM_ResolveClass");
  if (PrintJVMWarnings) warning("JVM_ResolveClass not implemented");
JVM_END


JVM_ENTRY(jboolean, JVM_KnownToNotExist(JNIEnv *env, jobject loader, const char *classname))
  JVMWrapper("JVM_KnownToNotExist");
#if INCLUDE_CDS
  return ClassLoaderExt::known_to_not_exist(env, loader, classname, CHECK_(false));
#else
  return false;
#endif
JVM_END


JVM_ENTRY(jobjectArray, JVM_GetResourceLookupCacheURLs(JNIEnv *env, jobject loader))
  JVMWrapper("JVM_GetResourceLookupCacheURLs");
#if INCLUDE_CDS
  return ClassLoaderExt::get_lookup_cache_urls(env, loader, CHECK_NULL);
#else
  return NULL;
#endif
JVM_END


JVM_ENTRY(jintArray, JVM_GetResourceLookupCache(JNIEnv *env, jobject loader, const char *resource_name))
  JVMWrapper("JVM_GetResourceLookupCache");
#if INCLUDE_CDS
  return ClassLoaderExt::get_lookup_cache(env, loader, resource_name, CHECK_NULL);
#else
  return NULL;
#endif
JVM_END


// Returns a class loaded by the bootstrap class loader; or null
// if not found.  ClassNotFoundException is not thrown.
//
// Rationale behind JVM_FindClassFromBootLoader
// a> JVM_FindClassFromClassLoader was never exported in the export tables.
// b> because of (a) java.dll has a direct dependecy on the  unexported
//    private symbol "_JVM_FindClassFromClassLoader@20".
// c> the launcher cannot use the private symbol as it dynamically opens
//    the entry point, so if something changes, the launcher will fail
//    unexpectedly at runtime, it is safest for the launcher to dlopen a
//    stable exported interface.
// d> re-exporting JVM_FindClassFromClassLoader as public, will cause its
//    signature to change from _JVM_FindClassFromClassLoader@20 to
//    JVM_FindClassFromClassLoader and will not be backward compatible
//    with older JDKs.
// Thus a public/stable exported entry point is the right solution,
// public here means public in linker semantics, and is exported only
// to the JDK, and is not intended to be a public API.

JVM_ENTRY(jclass, JVM_FindClassFromBootLoader(JNIEnv* env,
        const char* name))
  JVMWrapper2("JVM_FindClassFromBootLoader %s", name);

  // Java libraries should ensure that name is never null...
  if (name == NULL || (int)strlen(name) > Symbol::max_length()) {
    // It's impossible to create this class;  the name cannot fit
    // into the constant pool.
    return NULL;
  }

  TempNewSymbol h_name = SymbolTable::new_symbol(name, CHECK_NULL);
  Klass* k = SystemDictionary::resolve_or_null(h_name, CHECK_NULL);
  if (k == NULL) {
    return NULL;
  }

  if (TraceClassResolution) {
    trace_class_resolution(k);
  }
  return (jclass) JNIHandles::make_local(env, k->java_mirror());
JVM_END

// Not used; JVM_FindClassFromCaller replaces this.
JVM_ENTRY(jclass, JVM_FindClassFromClassLoader(JNIEnv* env, const char* name,
        jboolean init, jobject loader,
                jboolean throwError))
  JVMWrapper3("JVM_FindClassFromClassLoader %s throw %s", name,
              throwError ? "error" : "exception");
  // Java libraries should ensure that name is never null...
  if (name == NULL || (int)strlen(name) > Symbol::max_length()) {
    // It's impossible to create this class;  the name cannot fit
    // into the constant pool.
    if (throwError) {
      THROW_MSG_0(vmSymbols::java_lang_NoClassDefFoundError(), name);
    } else {
      THROW_MSG_0(vmSymbols::java_lang_ClassNotFoundException(), name);
    }
  }
  TempNewSymbol h_name = SymbolTable::new_symbol(name, CHECK_NULL);
  Handle h_loader(THREAD, JNIHandles::resolve(loader));
  jclass result = find_class_from_class_loader(env, h_name, init, h_loader,
                                               Handle(), throwError, THREAD);

  if (TraceClassResolution && result != NULL) {
    trace_class_resolution(java_lang_Class::as_Klass(JNIHandles::resolve_non_null(result)));
  }
  return result;
JVM_END

// Find a class with this name in this loader, using the caller's protection domain.
JVM_ENTRY(jclass, JVM_FindClassFromCaller(JNIEnv* env, const char* name,
        jboolean init, jobject loader,
                jclass caller))
  JVMWrapper2("JVM_FindClassFromCaller %s throws ClassNotFoundException", name);
  // Java libraries should ensure that name is never null...
  if (name == NULL || (int)strlen(name) > Symbol::max_length()) {
    // It's impossible to create this class;  the name cannot fit
    // into the constant pool.
    THROW_MSG_0(vmSymbols::java_lang_ClassNotFoundException(), name);
  }

  TempNewSymbol h_name = SymbolTable::new_symbol(name, CHECK_NULL);

  oop loader_oop = JNIHandles::resolve(loader);
  oop from_class = JNIHandles::resolve(caller);
  oop protection_domain = NULL;
  // If loader is null, shouldn't call ClassLoader.checkPackageAccess; otherwise get
  // NPE. Put it in another way, the bootstrap class loader has all permission and
  // thus no checkPackageAccess equivalence in the VM class loader.
  // The caller is also passed as NULL by the java code if there is no security
  // manager to avoid the performance cost of getting the calling class.
  if (from_class != NULL && loader_oop != NULL) {
    protection_domain = java_lang_Class::as_Klass(from_class)->protection_domain();
  }

  Handle h_loader(THREAD, loader_oop);
  Handle h_prot(THREAD, protection_domain);
  jclass result = find_class_from_class_loader(env, h_name, init, h_loader,
                                               h_prot, false, THREAD);

  if (TraceClassResolution && result != NULL) {
    trace_class_resolution(java_lang_Class::as_Klass(JNIHandles::resolve_non_null(result)));
  }
  return result;
JVM_END

JVM_ENTRY(jclass, JVM_FindClassFromClass(JNIEnv *env, const char *name,
        jboolean init, jclass from))
  JVMWrapper2("JVM_FindClassFromClass %s", name);
  if (name == NULL || (int)strlen(name) > Symbol::max_length()) {
    // It's impossible to create this class;  the name cannot fit
    // into the constant pool.
    THROW_MSG_0(vmSymbols::java_lang_NoClassDefFoundError(), name);
  }
  TempNewSymbol h_name = SymbolTable::new_symbol(name, CHECK_NULL);
  oop from_class_oop = JNIHandles::resolve(from);
  Klass* from_class = (from_class_oop == NULL)
                      ? (Klass*)NULL
                      : java_lang_Class::as_Klass(from_class_oop);
  oop class_loader = NULL;
  oop protection_domain = NULL;
  if (from_class != NULL) {
    class_loader = from_class->class_loader();
    protection_domain = from_class->protection_domain();
  }
  Handle h_loader(THREAD, class_loader);
  Handle h_prot  (THREAD, protection_domain);
  jclass result = find_class_from_class_loader(env, h_name, init, h_loader,
                                               h_prot, true, thread);

  if (TraceClassResolution && result != NULL) {
    // this function is generally only used for class loading during verification.
    ResourceMark rm;
    oop from_mirror = JNIHandles::resolve_non_null(from);
    Klass* from_class = java_lang_Class::as_Klass(from_mirror);
    const char * from_name = from_class->external_name();

    oop mirror = JNIHandles::resolve_non_null(result);
    Klass* to_class = java_lang_Class::as_Klass(mirror);
    const char * to = to_class->external_name();
    tty->print("RESOLVE %s %s (verification)\n", from_name, to);
  }

  return result;
JVM_END

//static void is_lock_held_by_thread(Handle loader, PerfCounter* counter, TRAPS) {
//  D_WARN_Unimplement;
//}

// common code for JVM_DefineClass() and JVM_DefineClassWithSource()
// and JVM_DefineClassWithSourceCond()
static jclass jvm_define_class_common(JNIEnv *env, const char *name,
                                      jobject loader, const jbyte *buf,
                                      jsize len, jobject pd, const char *source,
                                      jboolean verify, TRAPS) {
  D_WARN_Unimplement;
}


JVM_ENTRY(jclass, JVM_DefineClass(JNIEnv *env, const char *name, jobject loader, const jbyte *buf, jsize len, jobject pd))
  JVMWrapper2("JVM_DefineClass %s", name);

  return jvm_define_class_common(env, name, loader, buf, len, pd, NULL, true, THREAD);
JVM_END


JVM_ENTRY(jclass, JVM_DefineClassWithSource(JNIEnv *env, const char *name, jobject loader, const jbyte *buf, jsize len, jobject pd, const char *source))
  JVMWrapper2("JVM_DefineClassWithSource %s", name);

  return jvm_define_class_common(env, name, loader, buf, len, pd, source, true, THREAD);
JVM_END

JVM_ENTRY(jclass, JVM_DefineClassWithSourceCond(JNIEnv *env, const char *name,
        jobject loader, const jbyte *buf,
        jsize len, jobject pd,
        const char *source, jboolean verify))
  JVMWrapper2("JVM_DefineClassWithSourceCond %s", name);

  return jvm_define_class_common(env, name, loader, buf, len, pd, source, verify, THREAD);
JVM_END

JVM_ENTRY(jclass, JVM_FindLoadedClass(JNIEnv *env, jobject loader, jstring name))
  JVMWrapper("JVM_FindLoadedClass");
  ResourceMark rm(THREAD);

  Handle h_name (THREAD, JNIHandles::resolve_non_null(name));
  Handle string = java_lang_String::internalize_classname(h_name, CHECK_NULL);

  const char* str   = java_lang_String::as_utf8_string(string());
  // Sanity check, don't expect null
  if (str == NULL) return NULL;

  const int str_len = (int)strlen(str);
  if (str_len > Symbol::max_length()) {
    // It's impossible to create this class;  the name cannot fit
    // into the constant pool.
    return NULL;
  }
  TempNewSymbol klass_name = SymbolTable::new_symbol(str, str_len, CHECK_NULL);

  // Security Note:
  //   The Java level wrapper will perform the necessary security check allowing
  //   us to pass the NULL as the initiating class loader.
  Handle h_loader(THREAD, JNIHandles::resolve(loader));

  Klass* k = SystemDictionary::find_instance_or_array_klass(klass_name,
                                                            h_loader,
                                                            Handle(),
  CHECK_NULL);
#if INCLUDE_CDS
  if (k == NULL) {
    // If the class is not already loaded, try to see if it's in the shared
    // archive for the current classloader (h_loader).
    instanceKlassHandle ik = SystemDictionaryShared::find_or_load_shared_class(
        klass_name, h_loader, CHECK_NULL);
    k = ik();
  }
#endif
  return (k == NULL) ? NULL :
         (jclass) JNIHandles::make_local(env, k->java_mirror());
JVM_END


// Reflection support //////////////////////////////////////////////////////////////////////////////

JVM_ENTRY(jstring, JVM_GetClassName(JNIEnv *env, jclass cls))
  assert (cls != NULL, "illegal class");
  JVMWrapper("JVM_GetClassName");
#ifndef ENCLAVE_UNIX
  // JvmtiVMObjectAllocEventCollector oam;
  ResourceMark rm(THREAD);
#endif
  const char* name;
  if (java_lang_Class::is_primitive(JNIHandles::resolve(cls))) {
    name = type2name(java_lang_Class::primitive_type(JNIHandles::resolve(cls)));
  } else {
    // Consider caching interned string in Klass
    Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve(cls));
    assert(k->is_klass(), "just checking");
    name = k->external_name();
  }
#ifndef ENCLAVE_UNIX
  oop result = StringTable::intern((char*) name, CHECK_NULL);
#else
  oop result = EnclaveMemory::vm_new_string(THREAD, (char*)name);
#endif
  return (jstring) JNIHandles::make_local(env, result);
JVM_END


JVM_ENTRY(jobjectArray, JVM_GetClassInterfaces(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_GetClassInterfaces");
  // JvmtiVMObjectAllocEventCollector oam;
  oop mirror = JNIHandles::resolve_non_null(cls);

  // Special handling for primitive objects
  if (java_lang_Class::is_primitive(mirror)) {
    // Primitive objects does not have any interfaces
    objArrayOop r = oopFactory::new_objArray(SystemDictionary::Class_klass(), 0, CHECK_NULL);
    return (jobjectArray) JNIHandles::make_local(env, r);
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
  objArrayOop r = (oop) EnclaveMemory::heapMemory->klass_new_array(THREAD, SystemDictionary::Class_klass()->java_mirror(), size);
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


JVM_ENTRY(jobject, JVM_GetClassLoader(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_GetClassLoader");
  if (java_lang_Class::is_primitive(JNIHandles::resolve_non_null(cls))) {
    return NULL;
  }
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  oop loader = k->class_loader();
  return JNIHandles::make_local(env, loader);
JVM_END


JVM_QUICK_ENTRY(jboolean, JVM_IsInterface(JNIEnv *env, jclass cls))
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


JVM_ENTRY(jobjectArray, JVM_GetClassSigners(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_GetClassSigners");
  // JvmtiVMObjectAllocEventCollector oam;
  if (java_lang_Class::is_primitive(JNIHandles::resolve_non_null(cls))) {
    // There are no signers for primitive types
    return NULL;
  }

  objArrayOop signers = java_lang_Class::signers(JNIHandles::resolve_non_null(cls));

  // If there are no signers set in the class, or if the class
  // is an array, return NULL.
  if (signers == NULL) return NULL;

  // copy of the signers array
  Klass* element = ObjArrayKlass::cast(signers->klass())->element_klass();
  objArrayOop signers_copy = oopFactory::new_objArray(element, signers->length(), CHECK_NULL);
  for (int index = 0; index < signers->length(); index++) {
    signers_copy->obj_at_put(index, signers->obj_at(index));
  }

  // return the copy
  return (jobjectArray) JNIHandles::make_local(env, signers_copy);
JVM_END


JVM_ENTRY(void, JVM_SetClassSigners(JNIEnv *env, jclass cls, jobjectArray signers))
  JVMWrapper("JVM_SetClassSigners");
  if (!java_lang_Class::is_primitive(JNIHandles::resolve_non_null(cls))) {
    // This call is ignored for primitive types and arrays.
    // Signers are only set once, ClassLoader.java, and thus shouldn't
    // be called with an array.  Only the bootstrap loader creates arrays.
    Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
    if (k->oop_is_instance()) {
      java_lang_Class::set_signers(k->java_mirror(), objArrayOop(JNIHandles::resolve(signers)));
    }
  }
JVM_END


JVM_ENTRY(jobject, JVM_GetProtectionDomain(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_GetProtectionDomain");
  if (JNIHandles::resolve(cls) == NULL) {
    THROW_(vmSymbols::java_lang_NullPointerException(), NULL);
  }

  if (java_lang_Class::is_primitive(JNIHandles::resolve(cls))) {
    // Primitive types does not have a protection domain.
    return NULL;
  }

  oop pd = java_lang_Class::protection_domain(JNIHandles::resolve(cls));
  return (jobject) JNIHandles::make_local(env, pd);
JVM_END


static bool is_authorized(Handle context, instanceKlassHandle klass, TRAPS) {
  // If there is a security manager and protection domain, check the access
  // in the protection domain, otherwise it is authorized.
  if (java_lang_System::has_security_manager()) {

    // For bootstrapping, if pd implies method isn't in the JDK, allow
    // this context to revert to older behavior.
    // In this case the isAuthorized field in AccessControlContext is also not
    // present.
    if (Universe::protection_domain_implies_method() == NULL) {
      return true;
    }

    // Whitelist certain access control contexts
    if (java_security_AccessControlContext::is_authorized(context)) {
      return true;
    }

    oop prot = klass->protection_domain();
    if (prot != NULL) {
      // Call pd.implies(new SecurityPermission("createAccessControlContext"))
      // in the new wrapper.
      methodHandle m(THREAD, Universe::protection_domain_implies_method());
      Handle h_prot(THREAD, prot);
      JavaValue result(T_BOOLEAN);
      JavaCallArguments args(h_prot);
      JavaCalls::call(&result, m, &args, CHECK_false);
      return (result.get_jboolean() != 0);
    }
  }
  return true;
}

// Create an AccessControlContext with a protection domain with null codesource
// and null permissions - which gives no permissions.
oop create_dummy_access_control_context(TRAPS) {
  InstanceKlass* pd_klass = InstanceKlass::cast(SystemDictionary::ProtectionDomain_klass());
  Handle obj = pd_klass->allocate_instance_handle(CHECK_NULL);
  // Call constructor ProtectionDomain(null, null);
  JavaValue result(T_VOID);
  JavaCalls::call_special(&result, obj, KlassHandle(THREAD, pd_klass),
                          vmSymbols::object_initializer_name(),
                          vmSymbols::codesource_permissioncollection_signature(),
                          Handle(), Handle(), CHECK_NULL);

  // new ProtectionDomain[] {pd};
  objArrayOop context = oopFactory::new_objArray(pd_klass, 1, CHECK_NULL);
  context->obj_at_put(0, obj());

  // new AccessControlContext(new ProtectionDomain[] {pd})
  objArrayHandle h_context(THREAD, context);
  oop acc = java_security_AccessControlContext::create(h_context, false, Handle(), CHECK_NULL);
  return acc;
}

JVM_ENTRY(jobject, JVM_DoPrivileged(JNIEnv *env, jclass cls, jobject action, jobject context, jboolean wrapException))
  // TODO: execute with previleged
  JVMWrapper("JVM_DoPrivileged");

  if (action == NULL) {
    THROW_MSG_0(vmSymbols::java_lang_NullPointerException(), "Null action");
  }

  // Compute the frame initiating the do privileged operation and setup the privileged stack
  RegisterMap map(JavaThread::current());
  frame f = JavaThread::current()->last_frame();
  frame caller = f.sender_for_interpreter_frame(&map);

  Method* method        = caller.interpreter_frame_method();
  instanceKlassHandle klass (THREAD, method->method_holder());

  // Check that action object understands "Object run()"
//  Handle h_context;
//  if (context != NULL) {
//    h_context = Handle(THREAD, JNIHandles::resolve(context));
//    bool authorized = is_authorized(h_context, klass, CHECK_NULL);
//    if (!authorized) {
//      // Create an unprivileged access control object and call it's run function
//      // instead.
//      oop noprivs = create_dummy_access_control_context(CHECK_NULL);
//      h_context = Handle(THREAD, noprivs);
//    }
//  }

  // Check that action object understands "Object run()"
  Handle object (THREAD, JNIHandles::resolve(action));

  // get run() method
  Method* m_oop = object->klass()->uncached_lookup_method(
          vmSymbols::run_method_name(),
          vmSymbols::void_object_signature(),
          Klass::find_overpass);
  methodHandle m (THREAD, m_oop);
  if (m.is_null() || !m->is_method() || !m()->is_public() || m()->is_static()) {
    THROW_MSG_0(vmSymbols::java_lang_InternalError(), "No run method");
  }

  // invoke the Object run() in the action object. We cannot use call_interface here, since the static type
  // is not really known - it is either java.security.PrivilegedAction or java.security.PrivilegedExceptionAction
  Handle pending_exception;
  JavaValue result(T_OBJECT);
  JavaCallArguments args(object);
  JavaCalls::call(&result, m, &args, THREAD);

  if (HAS_PENDING_EXCEPTION) {
    pending_exception = Handle(THREAD, PENDING_EXCEPTION);
    CLEAR_PENDING_EXCEPTION;
    // JVMTI has already reported the pending exception
    // JVMTI internal flag reset is needed in order to report PrivilegedActionException
    if ( pending_exception->is_a(SystemDictionary::Exception_klass()) &&
         !pending_exception->is_a(SystemDictionary::RuntimeException_klass())) {
      // Throw a java.security.PrivilegedActionException(Exception e) exception
      JavaCallArguments args(pending_exception);
      THROW_ARG_0(vmSymbols::java_security_PrivilegedActionException(),
                  vmSymbols::exception_void_signature(),
                  &args);
    }
  }

  if (pending_exception.not_null()) THROW_OOP_0(pending_exception());
  return JNIHandles::make_local(env, (oop) result.get_jobject());
JVM_END


// Returns the inherited_access_control_context field of the running thread.
JVM_ENTRY(jobject, JVM_GetInheritedAccessControlContext(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_GetInheritedAccessControlContext");
  oop result = java_lang_Thread::inherited_access_control_context(thread->threadObj());
  return JNIHandles::make_local(env, result);
JVM_END

class RegisterArrayForGC {
private:
    JavaThread *_thread;
public:
    RegisterArrayForGC(JavaThread *thread, GrowableArray<oop>* array)  {
      _thread = thread;
    }

    ~RegisterArrayForGC() {
    }
};


JVM_ENTRY(jobject, JVM_GetStackAccessControlContext(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_GetStackAccessControlContext");
  D_WARN_Unimplement;
  return NULL;
JVM_END


JVM_QUICK_ENTRY(jboolean, JVM_IsArrayClass(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_IsArrayClass");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  return (k != NULL) && k->oop_is_array() ? true : false;
JVM_END


JVM_QUICK_ENTRY(jboolean, JVM_IsPrimitiveClass(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_IsPrimitiveClass");
  oop mirror = JNIHandles::resolve_non_null(cls);
  return (jboolean) java_lang_Class::is_primitive(mirror);
JVM_END


JVM_ENTRY(jclass, JVM_GetComponentType(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_GetComponentType");
  oop mirror = JNIHandles::resolve_non_null(cls);
  oop result = Reflection::array_component_type(mirror, CHECK_NULL);
  return (jclass) JNIHandles::make_local(env, result);
JVM_END


JVM_ENTRY(jint, JVM_GetClassModifiers(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_GetClassModifiers");
  if (java_lang_Class::is_primitive(JNIHandles::resolve_non_null(cls))) {
    // Primitive type
    return JVM_ACC_ABSTRACT | JVM_ACC_FINAL | JVM_ACC_PUBLIC;
  }

  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  debug_only(int computed_modifiers = k->compute_modifier_flags(CHECK_0));
  assert(k->modifier_flags() == computed_modifiers, "modifiers cache is OK");
  return k->modifier_flags();
JVM_END


// Inner class reflection ///////////////////////////////////////////////////////////////////////////////

JVM_ENTRY(jobjectArray, JVM_GetDeclaredClasses(JNIEnv *env, jclass ofClass))
  // JvmtiVMObjectAllocEventCollector oam;
  // ofClass is a reference to a java_lang_Class object. The mirror object
  // of an InstanceKlass

  if (java_lang_Class::is_primitive(JNIHandles::resolve_non_null(ofClass)) ||
      ! java_lang_Class::as_Klass(JNIHandles::resolve_non_null(ofClass))->oop_is_instance()) {
    oop result = oopFactory::new_objArray(SystemDictionary::Class_klass(), 0, CHECK_NULL);
    return (jobjectArray)JNIHandles::make_local(env, result);
  }

  instanceKlassHandle k(thread, java_lang_Class::as_Klass(JNIHandles::resolve_non_null(ofClass)));
  InnerClassesIterator iter(k);

  if (iter.length() == 0) {
    // Neither an inner nor outer class
    oop result = oopFactory::new_objArray(SystemDictionary::Class_klass(), 0, CHECK_NULL);
    return (jobjectArray)JNIHandles::make_local(env, result);
  }

  // find inner class info
  constantPoolHandle cp(thread, k->constants());
  int length = iter.length();

  // Allocate temp. result array
  objArrayOop r = oopFactory::new_objArray(SystemDictionary::Class_klass(), length/4, CHECK_NULL);
  objArrayHandle result (THREAD, r);
  int members = 0;

  for (; !iter.done(); iter.next()) {
    int ioff = iter.inner_class_info_index();
    int ooff = iter.outer_class_info_index();

    if (ioff != 0 && ooff != 0) {
      // Check to see if the name matches the class we're looking for
      // before attempting to find the class.
      if (cp->klass_name_at_matches(k, ooff)) {
        Klass* outer_klass = cp->klass_at(ooff, CHECK_NULL);
        if (outer_klass == k()) {
          Klass* ik = cp->klass_at(ioff, CHECK_NULL);
          instanceKlassHandle inner_klass (THREAD, ik);

          // Throws an exception if outer klass has not declared k as
          // an inner klass
          Reflection::check_for_inner_class(k, inner_klass, true, CHECK_NULL);

          result->obj_at_put(members, inner_klass->java_mirror());
          members++;
        }
      }
    }
  }

  if (members != length) {
    // Return array of right length
    objArrayOop res = oopFactory::new_objArray(SystemDictionary::Class_klass(), members, CHECK_NULL);
    for(int i = 0; i < members; i++) {
      res->obj_at_put(i, result->obj_at(i));
    }
    return (jobjectArray)JNIHandles::make_local(env, res);
  }

  return (jobjectArray)JNIHandles::make_local(env, result());
JVM_END


JVM_ENTRY(jclass, JVM_GetDeclaringClass(JNIEnv *env, jclass ofClass))
  {
    // ofClass is a reference to a java_lang_Class object.
    if (java_lang_Class::is_primitive(JNIHandles::resolve_non_null(ofClass)) ||
        ! java_lang_Class::as_Klass(JNIHandles::resolve_non_null(ofClass))->oop_is_instance()) {
      return NULL;
    }

    bool inner_is_member = false;
    Klass* outer_klass
            = InstanceKlass::cast(java_lang_Class::as_Klass(JNIHandles::resolve_non_null(ofClass))
            )->compute_enclosing_class(&inner_is_member, CHECK_NULL);
    if (outer_klass == NULL)  return NULL;  // already a top-level class
    if (!inner_is_member)  return NULL;     // an anonymous class (inside a method)
    return (jclass) JNIHandles::make_local(env, outer_klass->java_mirror());
  }
JVM_END

// should be in InstanceKlass.cpp, but is here for historical reasons
Klass* InstanceKlass::compute_enclosing_class_impl(instanceKlassHandle k,
                                                   bool* inner_is_member,
                                                   TRAPS) {
  Thread* thread = THREAD;
  InnerClassesIterator iter(k);
  if (iter.length() == 0) {
    // No inner class info => no declaring class
    return NULL;
  }

  constantPoolHandle i_cp(thread, k->constants());

  bool found = false;
  Klass* ok;
  instanceKlassHandle outer_klass;
  *inner_is_member = false;

  // Find inner_klass attribute
  for (; !iter.done() && !found; iter.next()) {
    int ioff = iter.inner_class_info_index();
    int ooff = iter.outer_class_info_index();
    int noff = iter.inner_name_index();
    if (ioff != 0) {
      // Check to see if the name matches the class we're looking for
      // before attempting to find the class.
      if (i_cp->klass_name_at_matches(k, ioff)) {
        Klass* inner_klass = i_cp->klass_at(ioff, CHECK_NULL);
        found = (k() == inner_klass);
        if (found && ooff != 0) {
          ok = i_cp->klass_at(ooff, CHECK_NULL);
          outer_klass = instanceKlassHandle(thread, ok);
          *inner_is_member = true;
        }
      }
    }
  }

  if (found && outer_klass.is_null()) {
    // It may be anonymous; try for that.
    int encl_method_class_idx = k->enclosing_method_class_index();
    if (encl_method_class_idx != 0) {
      ok = i_cp->klass_at(encl_method_class_idx, CHECK_NULL);
      outer_klass = instanceKlassHandle(thread, ok);
      *inner_is_member = false;
    }
  }

  // If no inner class attribute found for this class.
  if (outer_klass.is_null())  return NULL;

  // Throws an exception if outer klass has not declared k as an inner klass
  // We need evidence that each klass knows about the other, or else
  // the system could allow a spoof of an inner class to gain access rights.
  Reflection::check_for_inner_class(outer_klass, k, *inner_is_member, CHECK_NULL);
  return outer_klass();
}

JVM_ENTRY(jstring, JVM_GetClassSignature(JNIEnv *env, jclass cls))
  assert (cls != NULL, "illegal class");
  JVMWrapper("JVM_GetClassSignature");
  // JvmtiVMObjectAllocEventCollector oam;
  ResourceMark rm(THREAD);
  // Return null for arrays and primatives
  if (!java_lang_Class::is_primitive(JNIHandles::resolve(cls))) {
    Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve(cls));
    if (k->oop_is_instance()) {
      Symbol* sym = InstanceKlass::cast(k)->generic_signature();
      if (sym == NULL) return NULL;
      Handle str = java_lang_String::create_from_symbol(sym, CHECK_NULL);
      return (jstring) JNIHandles::make_local(env, str());
    }
  }
  return NULL;
JVM_END


JVM_ENTRY(jbyteArray, JVM_GetClassAnnotations(JNIEnv *env, jclass cls))
  assert (cls != NULL, "illegal class");
  JVMWrapper("JVM_GetClassAnnotations");

  // Return null for arrays and primitives
  if (!java_lang_Class::is_primitive(JNIHandles::resolve(cls))) {
    Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve(cls));
    if (k->oop_is_instance()) {
      typeArrayOop a = Annotations::make_java_array(InstanceKlass::cast(k)->class_annotations(), CHECK_NULL);
      return (jbyteArray) JNIHandles::make_local(env, a);
    }
  }
  return NULL;
JVM_END


static bool jvm_get_field_common(jobject field, fieldDescriptor& fd, TRAPS) {
  // some of this code was adapted from from jni_FromReflectedField

  oop reflected = JNIHandles::resolve_non_null(field);
  oop mirror    = java_lang_reflect_Field::clazz(reflected);
  Klass* k    = java_lang_Class::as_Klass(mirror);
  int slot      = java_lang_reflect_Field::slot(reflected);
  int modifiers = java_lang_reflect_Field::modifiers(reflected);

  KlassHandle kh(THREAD, k);
  intptr_t offset = InstanceKlass::cast(kh())->field_offset(slot);

  if (modifiers & JVM_ACC_STATIC) {
    // for static fields we only look in the current class
    if (!InstanceKlass::cast(kh())->find_local_field_from_offset(offset, true, &fd)) {
      assert(false, "cannot find static field");
      return false;
    }
  } else {
    // for instance fields we start with the current class and work
    // our way up through the superclass chain
    if (!InstanceKlass::cast(kh())->find_field_from_offset(offset, false, &fd)) {
      assert(false, "cannot find instance field");
      return false;
    }
  }
  return true;
}

JVM_ENTRY(jbyteArray, JVM_GetFieldAnnotations(JNIEnv *env, jobject field))
  // field is a handle to a java.lang.reflect.Field object
  assert(field != NULL, "illegal field");
  JVMWrapper("JVM_GetFieldAnnotations");

  fieldDescriptor fd;
  bool gotFd = jvm_get_field_common(field, fd, CHECK_NULL);
  if (!gotFd) {
    return NULL;
  }

  return (jbyteArray) JNIHandles::make_local(env, Annotations::make_java_array(fd.annotations(), THREAD));
JVM_END


static Method* jvm_get_method_common(jobject method) {
  // some of this code was adapted from from jni_FromReflectedMethod

  oop reflected = JNIHandles::resolve_non_null(method);
  oop mirror    = NULL;
  int slot      = 0;

  if (reflected->klass() == SystemDictionary::reflect_Constructor_klass()) {
    mirror = java_lang_reflect_Constructor::clazz(reflected);
    slot   = java_lang_reflect_Constructor::slot(reflected);
  } else {
    assert(reflected->klass() == SystemDictionary::reflect_Method_klass(),
           "wrong type");
    mirror = java_lang_reflect_Method::clazz(reflected);
    slot   = java_lang_reflect_Method::slot(reflected);
  }
  Klass* k = java_lang_Class::as_Klass(mirror);

  Method* m = InstanceKlass::cast(k)->method_with_idnum(slot);
  assert(m != NULL, "cannot find method");
  return m;  // caller has to deal with NULL in product mode
}


JVM_ENTRY(jbyteArray, JVM_GetMethodAnnotations(JNIEnv *env, jobject method))
  JVMWrapper("JVM_GetMethodAnnotations");

  // method is a handle to a java.lang.reflect.Method object
  Method* m = jvm_get_method_common(method);
  if (m == NULL) {
    return NULL;
  }

  return (jbyteArray) JNIHandles::make_local(env,
                                             Annotations::make_java_array(m->annotations(), THREAD));
JVM_END


JVM_ENTRY(jbyteArray, JVM_GetMethodDefaultAnnotationValue(JNIEnv *env, jobject method))
  JVMWrapper("JVM_GetMethodDefaultAnnotationValue");

  // method is a handle to a java.lang.reflect.Method object
  Method* m = jvm_get_method_common(method);
  if (m == NULL) {
    return NULL;
  }

  return (jbyteArray) JNIHandles::make_local(env,
                                             Annotations::make_java_array(m->annotation_default(), THREAD));
JVM_END


JVM_ENTRY(jbyteArray, JVM_GetMethodParameterAnnotations(JNIEnv *env, jobject method))
  JVMWrapper("JVM_GetMethodParameterAnnotations");

  // method is a handle to a java.lang.reflect.Method object
  Method* m = jvm_get_method_common(method);
  if (m == NULL) {
    return NULL;
  }

  return (jbyteArray) JNIHandles::make_local(env,
                                             Annotations::make_java_array(m->parameter_annotations(), THREAD));
JVM_END

/* Type use annotations support (JDK 1.8) */

JVM_ENTRY(jbyteArray, JVM_GetClassTypeAnnotations(JNIEnv *env, jclass cls))
  assert (cls != NULL, "illegal class");
  JVMWrapper("JVM_GetClassTypeAnnotations");
  ResourceMark rm(THREAD);
  // Return null for arrays and primitives
  if (!java_lang_Class::is_primitive(JNIHandles::resolve(cls))) {
    Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve(cls));
    if (k->oop_is_instance()) {
      AnnotationArray* type_annotations = InstanceKlass::cast(k)->class_type_annotations();
      if (type_annotations != NULL) {
        typeArrayOop a = Annotations::make_java_array(type_annotations, CHECK_NULL);
        return (jbyteArray) JNIHandles::make_local(env, a);
      }
    }
  }
  return NULL;
JVM_END

JVM_ENTRY(jbyteArray, JVM_GetMethodTypeAnnotations(JNIEnv *env, jobject method))
  assert (method != NULL, "illegal method");
  JVMWrapper("JVM_GetMethodTypeAnnotations");

  // method is a handle to a java.lang.reflect.Method object
  Method* m = jvm_get_method_common(method);
  if (m == NULL) {
    return NULL;
  }

  AnnotationArray* type_annotations = m->type_annotations();
  if (type_annotations != NULL) {
    typeArrayOop a = Annotations::make_java_array(type_annotations, CHECK_NULL);
    return (jbyteArray) JNIHandles::make_local(env, a);
  }

  return NULL;
JVM_END

JVM_ENTRY(jbyteArray, JVM_GetFieldTypeAnnotations(JNIEnv *env, jobject field))
  assert (field != NULL, "illegal field");
  JVMWrapper("JVM_GetFieldTypeAnnotations");

  fieldDescriptor fd;
  bool gotFd = jvm_get_field_common(field, fd, CHECK_NULL);
  if (!gotFd) {
    return NULL;
  }

  return (jbyteArray) JNIHandles::make_local(env, Annotations::make_java_array(fd.type_annotations(), THREAD));
JVM_END

static void bounds_check(constantPoolHandle cp, jint index, TRAPS) {
  if (!cp->is_within_bounds(index)) {
    THROW_MSG(vmSymbols::java_lang_IllegalArgumentException(), "Constant pool index out of bounds");
  }
}

JVM_ENTRY(jobjectArray, JVM_GetMethodParameters(JNIEnv *env, jobject method))
  {
    JVMWrapper("JVM_GetMethodParameters");
    // method is a handle to a java.lang.reflect.Method object
    Method* method_ptr = jvm_get_method_common(method);
    methodHandle mh (THREAD, method_ptr);
    Handle reflected_method (THREAD, JNIHandles::resolve_non_null(method));
    const int num_params = mh->method_parameters_length();

    if (0 != num_params) {
      // make sure all the symbols are properly formatted
      for (int i = 0; i < num_params; i++) {
        MethodParametersElement* params = mh->method_parameters_start();
        int index = params[i].name_cp_index;
        bounds_check(mh->constants(), index, CHECK_NULL);

        if (0 != index && !mh->constants()->tag_at(index).is_utf8()) {
          THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(),
                      "Wrong type at constant pool index");
        }

      }

      objArrayOop result_oop = oopFactory::new_objArray(SystemDictionary::reflect_Parameter_klass(), num_params, CHECK_NULL);
      objArrayHandle result (THREAD, result_oop);

      for (int i = 0; i < num_params; i++) {
        MethodParametersElement* params = mh->method_parameters_start();
        // For a 0 index, give a NULL symbol
        Symbol* sym = 0 != params[i].name_cp_index ?
                      mh->constants()->symbol_at(params[i].name_cp_index) : NULL;
        int flags = params[i].flags;
        oop param = Reflection::new_parameter(reflected_method, i, sym,
                                              flags, CHECK_NULL);
        result->obj_at_put(i, param);
      }
      return (jobjectArray)JNIHandles::make_local(env, result());
    } else {
      return (jobjectArray)NULL;
    }
  }
JVM_END

// New (JDK 1.4) reflection implementation /////////////////////////////////////

JVM_ENTRY(jobjectArray, JVM_GetClassDeclaredFields(JNIEnv *env, jclass ofClass, jboolean publicOnly))
  {
    JVMWrapper("JVM_GetClassDeclaredFields");
    // JvmtiVMObjectAllocEventCollector oam;

    // Exclude primitive types and array types
    if (java_lang_Class::is_primitive(JNIHandles::resolve_non_null(ofClass)) ||
        java_lang_Class::as_Klass(JNIHandles::resolve_non_null(ofClass))->oop_is_array()) {
      // Return empty array
      oop res = oopFactory::new_objArray(SystemDictionary::reflect_Field_klass(), 0, CHECK_NULL);
      return (jobjectArray) JNIHandles::make_local(env, res);
    }

    instanceKlassHandle k(THREAD, java_lang_Class::as_Klass(JNIHandles::resolve_non_null(ofClass)));
    constantPoolHandle cp(THREAD, k->constants());

    // Ensure class is linked
    k->link_class(CHECK_NULL);

    // 4496456 We need to filter out java.lang.Throwable.backtrace
    bool skip_backtrace = false;

    // Allocate result
    int num_fields;

    if (publicOnly) {
      num_fields = 0;
      for (JavaFieldStream fs(k()); !fs.done(); fs.next()) {
        if (fs.access_flags().is_public()) ++num_fields;
      }
    } else {
      num_fields = k->java_fields_count();

      if (k() == SystemDictionary::Throwable_klass()) {
        num_fields--;
        skip_backtrace = true;
      }
    }

    objArrayOop r = oopFactory::new_objArray(SystemDictionary::reflect_Field_klass(), num_fields, CHECK_NULL);
    objArrayHandle result (THREAD, r);

    int out_idx = 0;
    fieldDescriptor fd;
    for (JavaFieldStream fs(k); !fs.done(); fs.next()) {
      if (skip_backtrace) {
        // 4496456 skip java.lang.Throwable.backtrace
        int offset = fs.offset();
        if (offset == java_lang_Throwable::get_backtrace_offset()) continue;
      }

      if (!publicOnly || fs.access_flags().is_public()) {
        fd.reinitialize(k(), fs.index());
        oop field = Reflection::new_field(&fd, UseNewReflection, CHECK_NULL);
        result->obj_at_put(out_idx, field);
        ++out_idx;
      }
    }
    assert(out_idx == num_fields, "just checking");
    return (jobjectArray) JNIHandles::make_local(env, result());
  }
JVM_END

static bool select_method(methodHandle method, bool want_constructor) {
  if (want_constructor) {
    return (method->is_initializer() && !method->is_static());
  } else {
    return  (!method->is_initializer() && !method->is_overpass());
  }
}

static jobjectArray get_class_declared_methods_helper(
        JNIEnv *env,
        jclass ofClass, jboolean publicOnly,
        bool want_constructor,
        Klass* klass, TRAPS) {

  // JvmtiVMObjectAllocEventCollector oam;

  // Exclude primitive types and array types
  if (java_lang_Class::is_primitive(JNIHandles::resolve_non_null(ofClass))
      || java_lang_Class::as_Klass(JNIHandles::resolve_non_null(ofClass))->oop_is_array()) {
    // Return empty array
    oop res = oopFactory::new_objArray(klass, 0, CHECK_NULL);
    return (jobjectArray) JNIHandles::make_local(env, res);
  }

  instanceKlassHandle k(THREAD, java_lang_Class::as_Klass(JNIHandles::resolve_non_null(ofClass)));

  // Ensure class is linked
  k->link_class(CHECK_NULL);

  Array<Method*>* methods = k->methods();
  int methods_length = methods->length();

  // Save original method_idnum in case of redefinition, which can change
  // the idnum of obsolete methods.  The new method will have the same idnum
  // but if we refresh the methods array, the counts will be wrong.
  ResourceMark rm(THREAD);
  GrowableArray<int>* idnums = new GrowableArray<int>(methods_length);
  int num_methods = 0;

  for (int i = 0; i < methods_length; i++) {
    methodHandle method(THREAD, methods->at(i));
    if (select_method(method, want_constructor)) {
      if (!publicOnly || method->is_public()) {
        idnums->push(method->method_idnum());
        ++num_methods;
      }
    }
  }

  // Allocate result
  objArrayOop r = oopFactory::new_objArray(klass, num_methods, CHECK_NULL);
  objArrayHandle result (THREAD, r);

  // Now just put the methods that we selected above, but go by their idnum
  // in case of redefinition.  The methods can be redefined at any safepoint,
  // so above when allocating the oop array and below when creating reflect
  // objects.
  for (int i = 0; i < num_methods; i++) {
    methodHandle method(THREAD, k->method_with_idnum(idnums->at(i)));
    if (method.is_null()) {
      // Method may have been deleted and seems this API can handle null
      // Otherwise should probably put a method that throws NSME
      result->obj_at_put(i, NULL);
    } else {
      oop m;
      if (want_constructor) {
        m = Reflection::new_constructor(method, CHECK_NULL);
      } else {
        m = Reflection::new_method(method, UseNewReflection, false, CHECK_NULL);
      }
      result->obj_at_put(i, m);
    }
  }

  return (jobjectArray) JNIHandles::make_local(env, result());
}

JVM_ENTRY(jobjectArray, JVM_GetClassDeclaredMethods(JNIEnv *env, jclass ofClass, jboolean publicOnly))
  {
    JVMWrapper("JVM_GetClassDeclaredMethods");
    return get_class_declared_methods_helper(env, ofClass, publicOnly,
            /*want_constructor*/ false,
                                             SystemDictionary::reflect_Method_klass(), THREAD);
  }
JVM_END

JVM_ENTRY(jobjectArray, JVM_GetClassDeclaredConstructors(JNIEnv *env, jclass ofClass, jboolean publicOnly))
  {
    JVMWrapper("JVM_GetClassDeclaredConstructors");
    return get_class_declared_methods_helper(env, ofClass, publicOnly,
            /*want_constructor*/ true,
                                             SystemDictionary::reflect_Constructor_klass(), THREAD);
  }
JVM_END

JVM_ENTRY(jint, JVM_GetClassAccessFlags(JNIEnv *env, jclass cls))
  {
    JVMWrapper("JVM_GetClassAccessFlags");
    if (java_lang_Class::is_primitive(JNIHandles::resolve_non_null(cls))) {
      // Primitive type
      return JVM_ACC_ABSTRACT | JVM_ACC_FINAL | JVM_ACC_PUBLIC;
    }

    Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
    return k->access_flags().as_int() & JVM_ACC_WRITTEN_FLAGS;
  }
JVM_END


// Constant pool access //////////////////////////////////////////////////////////

JVM_ENTRY(jobject, JVM_GetClassConstantPool(JNIEnv *env, jclass cls))
  {
    JVMWrapper("JVM_GetClassConstantPool");
    // JvmtiVMObjectAllocEventCollector oam;

    // Return null for primitives and arrays
    if (!java_lang_Class::is_primitive(JNIHandles::resolve_non_null(cls))) {
      Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
      if (k->oop_is_instance()) {
        instanceKlassHandle k_h(THREAD, k);
        Handle jcp = sun_reflect_ConstantPool::create(CHECK_NULL);
        sun_reflect_ConstantPool::set_cp(jcp(), k_h->constants());
        return JNIHandles::make_local(jcp());
      }
    }
    return NULL;
  }
JVM_END


JVM_ENTRY(jint, JVM_ConstantPoolGetSize(JNIEnv *env, jobject obj, jobject unused))
  {
    JVMWrapper("JVM_ConstantPoolGetSize");
    constantPoolHandle cp = constantPoolHandle(THREAD, sun_reflect_ConstantPool::get_cp(JNIHandles::resolve_non_null(obj)));
    return cp->length();
  }
JVM_END


JVM_ENTRY(jclass, JVM_ConstantPoolGetClassAt(JNIEnv *env, jobject obj, jobject unused, jint index))
  {
    JVMWrapper("JVM_ConstantPoolGetClassAt");
    constantPoolHandle cp = constantPoolHandle(THREAD, sun_reflect_ConstantPool::get_cp(JNIHandles::resolve_non_null(obj)));
    bounds_check(cp, index, CHECK_NULL);
    constantTag tag = cp->tag_at(index);
    if (!tag.is_klass() && !tag.is_unresolved_klass()) {
      THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "Wrong type at constant pool index");
    }
    Klass* k = cp->klass_at(index, CHECK_NULL);
    return (jclass) JNIHandles::make_local(k->java_mirror());
  }
JVM_END

JVM_ENTRY(jclass, JVM_ConstantPoolGetClassAtIfLoaded(JNIEnv *env, jobject obj, jobject unused, jint index))
  {
    JVMWrapper("JVM_ConstantPoolGetClassAtIfLoaded");
    constantPoolHandle cp = constantPoolHandle(THREAD, sun_reflect_ConstantPool::get_cp(JNIHandles::resolve_non_null(obj)));
    bounds_check(cp, index, CHECK_NULL);
    constantTag tag = cp->tag_at(index);
    if (!tag.is_klass() && !tag.is_unresolved_klass()) {
      THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "Wrong type at constant pool index");
    }
    Klass* k = ConstantPool::klass_at_if_loaded(cp, index);
    if (k == NULL) return NULL;
    return (jclass) JNIHandles::make_local(k->java_mirror());
  }
JVM_END

static jobject get_method_at_helper(constantPoolHandle cp, jint index, bool force_resolution, TRAPS) {
  constantTag tag = cp->tag_at(index);
  if (!tag.is_method() && !tag.is_interface_method()) {
    THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "Wrong type at constant pool index");
  }
  int klass_ref  = cp->uncached_klass_ref_index_at(index);
  Klass* k_o;
  if (force_resolution) {
    k_o = cp->klass_at(klass_ref, CHECK_NULL);
  } else {
    k_o = ConstantPool::klass_at_if_loaded(cp, klass_ref);
    if (k_o == NULL) return NULL;
  }
  instanceKlassHandle k(THREAD, k_o);
  Symbol* name = cp->uncached_name_ref_at(index);
  Symbol* sig  = cp->uncached_signature_ref_at(index);
  methodHandle m (THREAD, k->find_method(name, sig));
  if (m.is_null()) {
    THROW_MSG_0(vmSymbols::java_lang_RuntimeException(), "Unable to look up method in target class");
  }
  oop method;
  if (!m->is_initializer() || m->is_static()) {
    method = Reflection::new_method(m, true, true, CHECK_NULL);
  } else {
    method = Reflection::new_constructor(m, CHECK_NULL);
  }
  return JNIHandles::make_local(method);
}

JVM_ENTRY(jobject, JVM_ConstantPoolGetMethodAt(JNIEnv *env, jobject obj, jobject unused, jint index))
  {
    JVMWrapper("JVM_ConstantPoolGetMethodAt");
    // JvmtiVMObjectAllocEventCollector oam;
    constantPoolHandle cp = constantPoolHandle(THREAD, sun_reflect_ConstantPool::get_cp(JNIHandles::resolve_non_null(obj)));
    bounds_check(cp, index, CHECK_NULL);
    jobject res = get_method_at_helper(cp, index, true, CHECK_NULL);
    return res;
  }
JVM_END

JVM_ENTRY(jobject, JVM_ConstantPoolGetMethodAtIfLoaded(JNIEnv *env, jobject obj, jobject unused, jint index))
  {
    JVMWrapper("JVM_ConstantPoolGetMethodAtIfLoaded");
    // JvmtiVMObjectAllocEventCollector oam;
    constantPoolHandle cp = constantPoolHandle(THREAD, sun_reflect_ConstantPool::get_cp(JNIHandles::resolve_non_null(obj)));
    bounds_check(cp, index, CHECK_NULL);
    jobject res = get_method_at_helper(cp, index, false, CHECK_NULL);
    return res;
  }
JVM_END

static jobject get_field_at_helper(constantPoolHandle cp, jint index, bool force_resolution, TRAPS) {
  constantTag tag = cp->tag_at(index);
  if (!tag.is_field()) {
    THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "Wrong type at constant pool index");
  }
  int klass_ref  = cp->uncached_klass_ref_index_at(index);
  Klass* k_o;
  if (force_resolution) {
    k_o = cp->klass_at(klass_ref, CHECK_NULL);
  } else {
    k_o = ConstantPool::klass_at_if_loaded(cp, klass_ref);
    if (k_o == NULL) return NULL;
  }
  instanceKlassHandle k(THREAD, k_o);
  Symbol* name = cp->uncached_name_ref_at(index);
  Symbol* sig  = cp->uncached_signature_ref_at(index);
  fieldDescriptor fd;
  Klass* target_klass = k->find_field(name, sig, &fd);
  if (target_klass == NULL) {
    THROW_MSG_0(vmSymbols::java_lang_RuntimeException(), "Unable to look up field in target class");
  }
  oop field = Reflection::new_field(&fd, true, CHECK_NULL);
  return JNIHandles::make_local(field);
}

JVM_ENTRY(jobject, JVM_ConstantPoolGetFieldAt(JNIEnv *env, jobject obj, jobject unusedl, jint index))
  {
    JVMWrapper("JVM_ConstantPoolGetFieldAt");
    // JvmtiVMObjectAllocEventCollector oam;
    constantPoolHandle cp = constantPoolHandle(THREAD, sun_reflect_ConstantPool::get_cp(JNIHandles::resolve_non_null(obj)));
    bounds_check(cp, index, CHECK_NULL);
    jobject res = get_field_at_helper(cp, index, true, CHECK_NULL);
    return res;
  }
JVM_END

JVM_ENTRY(jobject, JVM_ConstantPoolGetFieldAtIfLoaded(JNIEnv *env, jobject obj, jobject unused, jint index))
  {
    JVMWrapper("JVM_ConstantPoolGetFieldAtIfLoaded");
    // JvmtiVMObjectAllocEventCollector oam;
    constantPoolHandle cp = constantPoolHandle(THREAD, sun_reflect_ConstantPool::get_cp(JNIHandles::resolve_non_null(obj)));
    bounds_check(cp, index, CHECK_NULL);
    jobject res = get_field_at_helper(cp, index, false, CHECK_NULL);
    return res;
  }
JVM_END

JVM_ENTRY(jobjectArray, JVM_ConstantPoolGetMemberRefInfoAt(JNIEnv *env, jobject obj, jobject unused, jint index))
  {
    JVMWrapper("JVM_ConstantPoolGetMemberRefInfoAt");
    // JvmtiVMObjectAllocEventCollector oam;
    constantPoolHandle cp = constantPoolHandle(THREAD, sun_reflect_ConstantPool::get_cp(JNIHandles::resolve_non_null(obj)));
    bounds_check(cp, index, CHECK_NULL);
    constantTag tag = cp->tag_at(index);
    if (!tag.is_field_or_method()) {
      THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "Wrong type at constant pool index");
    }
    int klass_ref = cp->uncached_klass_ref_index_at(index);
    Symbol*  klass_name  = cp->klass_name_at(klass_ref);
    Symbol*  member_name = cp->uncached_name_ref_at(index);
    Symbol*  member_sig  = cp->uncached_signature_ref_at(index);
    objArrayOop  dest_o = oopFactory::new_objArray(SystemDictionary::String_klass(), 3, CHECK_NULL);
    objArrayHandle dest(THREAD, dest_o);
    Handle str = java_lang_String::create_from_symbol(klass_name, CHECK_NULL);
    dest->obj_at_put(0, str());
    str = java_lang_String::create_from_symbol(member_name, CHECK_NULL);
    dest->obj_at_put(1, str());
    str = java_lang_String::create_from_symbol(member_sig, CHECK_NULL);
    dest->obj_at_put(2, str());
    return (jobjectArray) JNIHandles::make_local(dest());
  }
JVM_END

JVM_ENTRY(jint, JVM_ConstantPoolGetIntAt(JNIEnv *env, jobject obj, jobject unused, jint index))
  {
    JVMWrapper("JVM_ConstantPoolGetIntAt");
    constantPoolHandle cp = constantPoolHandle(THREAD, sun_reflect_ConstantPool::get_cp(JNIHandles::resolve_non_null(obj)));
    bounds_check(cp, index, CHECK_0);
    constantTag tag = cp->tag_at(index);
    if (!tag.is_int()) {
      THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "Wrong type at constant pool index");
    }
    return cp->int_at(index);
  }
JVM_END

JVM_ENTRY(jlong, JVM_ConstantPoolGetLongAt(JNIEnv *env, jobject obj, jobject unused, jint index))
  {
    JVMWrapper("JVM_ConstantPoolGetLongAt");
    constantPoolHandle cp = constantPoolHandle(THREAD, sun_reflect_ConstantPool::get_cp(JNIHandles::resolve_non_null(obj)));
    bounds_check(cp, index, CHECK_(0L));
    constantTag tag = cp->tag_at(index);
    if (!tag.is_long()) {
      THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "Wrong type at constant pool index");
    }
    return cp->long_at(index);
  }
JVM_END

JVM_ENTRY(jfloat, JVM_ConstantPoolGetFloatAt(JNIEnv *env, jobject obj, jobject unused, jint index))
  {
    JVMWrapper("JVM_ConstantPoolGetFloatAt");
    constantPoolHandle cp = constantPoolHandle(THREAD, sun_reflect_ConstantPool::get_cp(JNIHandles::resolve_non_null(obj)));
    bounds_check(cp, index, CHECK_(0.0f));
    constantTag tag = cp->tag_at(index);
    if (!tag.is_float()) {
      THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "Wrong type at constant pool index");
    }
    return cp->float_at(index);
  }
JVM_END

JVM_ENTRY(jdouble, JVM_ConstantPoolGetDoubleAt(JNIEnv *env, jobject obj, jobject unused, jint index))
  {
    JVMWrapper("JVM_ConstantPoolGetDoubleAt");
    constantPoolHandle cp = constantPoolHandle(THREAD, sun_reflect_ConstantPool::get_cp(JNIHandles::resolve_non_null(obj)));
    bounds_check(cp, index, CHECK_(0.0));
    constantTag tag = cp->tag_at(index);
    if (!tag.is_double()) {
      THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "Wrong type at constant pool index");
    }
    return cp->double_at(index);
  }
JVM_END

JVM_ENTRY(jstring, JVM_ConstantPoolGetStringAt(JNIEnv *env, jobject obj, jobject unused, jint index))
  {
    JVMWrapper("JVM_ConstantPoolGetStringAt");
    constantPoolHandle cp = constantPoolHandle(THREAD, sun_reflect_ConstantPool::get_cp(JNIHandles::resolve_non_null(obj)));
    bounds_check(cp, index, CHECK_NULL);
    constantTag tag = cp->tag_at(index);
    if (!tag.is_string()) {
      THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "Wrong type at constant pool index");
    }
    oop str = cp->string_at(index, CHECK_NULL);
    return (jstring) JNIHandles::make_local(str);
  }
JVM_END

JVM_ENTRY(jstring, JVM_ConstantPoolGetUTF8At(JNIEnv *env, jobject obj, jobject unused, jint index))
  {
    JVMWrapper("JVM_ConstantPoolGetUTF8At");
    // JvmtiVMObjectAllocEventCollector oam;
    constantPoolHandle cp = constantPoolHandle(THREAD, sun_reflect_ConstantPool::get_cp(JNIHandles::resolve_non_null(obj)));
    bounds_check(cp, index, CHECK_NULL);
    constantTag tag = cp->tag_at(index);
    if (!tag.is_symbol()) {
      THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "Wrong type at constant pool index");
    }
    Symbol* sym = cp->symbol_at(index);
    Handle str = java_lang_String::create_from_symbol(sym, CHECK_NULL);
    return (jstring) JNIHandles::make_local(str());
  }
JVM_END


// Assertion support. //////////////////////////////////////////////////////////

JVM_ENTRY(jboolean, JVM_DesiredAssertionStatus(JNIEnv *env, jclass unused, jclass cls))
  JVMWrapper("JVM_DesiredAssertionStatus");
  D_WARN_Unimplement;
  return false;
JVM_END


// Return a new AssertionStatusDirectives object with the fields filled in with
// command-line assertion arguments (i.e., -ea, -da).
JVM_ENTRY(jobject, JVM_AssertionStatusDirectives(JNIEnv *env, jclass unused))
  JVMWrapper("JVM_AssertionStatusDirectives");
  D_WARN_Unimplement;
  return NULL;
JVM_END

// Verification ////////////////////////////////////////////////////////////////////////////////

// Reflection for the verifier /////////////////////////////////////////////////////////////////

// RedefineClasses support: bug 6214132 caused verification to fail.
// All functions from this section should call the jvmtiThreadSate function:
//   Klass* class_to_verify_considering_redefinition(Klass* klass).
// The function returns a Klass* of the _scratch_class if the verifier
// was invoked in the middle of the class redefinition.
// Otherwise it returns its argument value which is the _the_class Klass*.
// Please, refer to the description in the jvmtiThreadSate.hpp.

JVM_ENTRY(const char*, JVM_GetClassNameUTF(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_GetClassNameUTF");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  return k->name()->as_utf8();
JVM_END


JVM_QUICK_ENTRY(void, JVM_GetClassCPTypes(JNIEnv *env, jclass cls, unsigned char *types))
  JVMWrapper("JVM_GetClassCPTypes");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  // types will have length zero if this is not an InstanceKlass
  // (length is determined by call to JVM_GetClassCPEntriesCount)
  if (k->oop_is_instance()) {
    ConstantPool* cp = InstanceKlass::cast(k)->constants();
    for (int index = cp->length() - 1; index >= 0; index--) {
      constantTag tag = cp->tag_at(index);
      types[index] = (tag.is_unresolved_klass()) ? JVM_CONSTANT_Class : tag.value();
    }
  }
JVM_END


JVM_QUICK_ENTRY(jint, JVM_GetClassCPEntriesCount(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_GetClassCPEntriesCount");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  if (!k->oop_is_instance())
    return 0;
  return InstanceKlass::cast(k)->constants()->length();
JVM_END


JVM_QUICK_ENTRY(jint, JVM_GetClassFieldsCount(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_GetClassFieldsCount");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  if (!k->oop_is_instance())
    return 0;
  return InstanceKlass::cast(k)->java_fields_count();
JVM_END


JVM_QUICK_ENTRY(jint, JVM_GetClassMethodsCount(JNIEnv *env, jclass cls))
  JVMWrapper("JVM_GetClassMethodsCount");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  if (!k->oop_is_instance())
    return 0;
  return InstanceKlass::cast(k)->methods()->length();
JVM_END


// The following methods, used for the verifier, are never called with
// array klasses, so a direct cast to InstanceKlass is safe.
// Typically, these methods are called in a loop with bounds determined
// by the results of JVM_GetClass{Fields,Methods}Count, which return
// zero for arrays.
JVM_QUICK_ENTRY(void, JVM_GetMethodIxExceptionIndexes(JNIEnv *env, jclass cls, jint method_index, unsigned short *exceptions))
  JVMWrapper("JVM_GetMethodIxExceptionIndexes");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  Method* method = InstanceKlass::cast(k)->methods()->at(method_index);
  int length = method->checked_exceptions_length();
  if (length > 0) {
    CheckedExceptionElement* table= method->checked_exceptions_start();
    for (int i = 0; i < length; i++) {
      exceptions[i] = table[i].class_cp_index;
    }
  }
JVM_END


JVM_QUICK_ENTRY(jint, JVM_GetMethodIxExceptionsCount(JNIEnv *env, jclass cls, jint method_index))
  JVMWrapper("JVM_GetMethodIxExceptionsCount");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  Method* method = InstanceKlass::cast(k)->methods()->at(method_index);
  return method->checked_exceptions_length();
JVM_END


JVM_QUICK_ENTRY(void, JVM_GetMethodIxByteCode(JNIEnv *env, jclass cls, jint method_index, unsigned char *code))
  JVMWrapper("JVM_GetMethodIxByteCode");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  Method* method = InstanceKlass::cast(k)->methods()->at(method_index);
  memcpy(code, method->code_base(), method->code_size());
JVM_END


JVM_QUICK_ENTRY(jint, JVM_GetMethodIxByteCodeLength(JNIEnv *env, jclass cls, jint method_index))
  JVMWrapper("JVM_GetMethodIxByteCodeLength");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  Method* method = InstanceKlass::cast(k)->methods()->at(method_index);
  return method->code_size();
JVM_END


JVM_QUICK_ENTRY(void, JVM_GetMethodIxExceptionTableEntry(JNIEnv *env, jclass cls, jint method_index, jint entry_index, JVM_ExceptionTableEntryType *entry))
  JVMWrapper("JVM_GetMethodIxExceptionTableEntry");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  Method* method = InstanceKlass::cast(k)->methods()->at(method_index);
  ExceptionTable extable(method);
  entry->start_pc   = extable.start_pc(entry_index);
  entry->end_pc     = extable.end_pc(entry_index);
  entry->handler_pc = extable.handler_pc(entry_index);
  entry->catchType  = extable.catch_type_index(entry_index);
JVM_END


JVM_QUICK_ENTRY(jint, JVM_GetMethodIxExceptionTableLength(JNIEnv *env, jclass cls, int method_index))
  JVMWrapper("JVM_GetMethodIxExceptionTableLength");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  Method* method = InstanceKlass::cast(k)->methods()->at(method_index);
  return method->exception_table_length();
JVM_END


JVM_QUICK_ENTRY(jint, JVM_GetMethodIxModifiers(JNIEnv *env, jclass cls, int method_index))
  JVMWrapper("JVM_GetMethodIxModifiers");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  Method* method = InstanceKlass::cast(k)->methods()->at(method_index);
  return method->access_flags().as_int() & JVM_RECOGNIZED_METHOD_MODIFIERS;
JVM_END


JVM_QUICK_ENTRY(jint, JVM_GetFieldIxModifiers(JNIEnv *env, jclass cls, int field_index))
  JVMWrapper("JVM_GetFieldIxModifiers");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  return InstanceKlass::cast(k)->field_access_flags(field_index) & JVM_RECOGNIZED_FIELD_MODIFIERS;
JVM_END


JVM_QUICK_ENTRY(jint, JVM_GetMethodIxLocalsCount(JNIEnv *env, jclass cls, int method_index))
  JVMWrapper("JVM_GetMethodIxLocalsCount");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  Method* method = InstanceKlass::cast(k)->methods()->at(method_index);
  return method->max_locals();
JVM_END


JVM_QUICK_ENTRY(jint, JVM_GetMethodIxArgsSize(JNIEnv *env, jclass cls, int method_index))
  JVMWrapper("JVM_GetMethodIxArgsSize");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  Method* method = InstanceKlass::cast(k)->methods()->at(method_index);
  return method->size_of_parameters();
JVM_END


JVM_QUICK_ENTRY(jint, JVM_GetMethodIxMaxStack(JNIEnv *env, jclass cls, int method_index))
  JVMWrapper("JVM_GetMethodIxMaxStack");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  Method* method = InstanceKlass::cast(k)->methods()->at(method_index);
  return method->verifier_max_stack();
JVM_END


JVM_QUICK_ENTRY(jboolean, JVM_IsConstructorIx(JNIEnv *env, jclass cls, int method_index))
  JVMWrapper("JVM_IsConstructorIx");
  ResourceMark rm(THREAD);
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  Method* method = InstanceKlass::cast(k)->methods()->at(method_index);
  return method->name() == vmSymbols::object_initializer_name();
JVM_END


JVM_QUICK_ENTRY(jboolean, JVM_IsVMGeneratedMethodIx(JNIEnv *env, jclass cls, int method_index))
  JVMWrapper("JVM_IsVMGeneratedMethodIx");
  ResourceMark rm(THREAD);
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  Method* method = InstanceKlass::cast(k)->methods()->at(method_index);
  return method->is_overpass();
JVM_END

JVM_ENTRY(const char*, JVM_GetMethodIxNameUTF(JNIEnv *env, jclass cls, jint method_index))
  JVMWrapper("JVM_GetMethodIxIxUTF");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  Method* method = InstanceKlass::cast(k)->methods()->at(method_index);
  return method->name()->as_utf8();
JVM_END


JVM_ENTRY(const char*, JVM_GetMethodIxSignatureUTF(JNIEnv *env, jclass cls, jint method_index))
  JVMWrapper("JVM_GetMethodIxSignatureUTF");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  Method* method = InstanceKlass::cast(k)->methods()->at(method_index);
  return method->signature()->as_utf8();
JVM_END

/**
 * All of these JVM_GetCP-xxx methods are used by the old verifier to
 * read entries in the constant pool.  Since the old verifier always
 * works on a copy of the code, it will not see any rewriting that
 * may possibly occur in the middle of verification.  So it is important
 * that nothing it calls tries to use the cpCache instead of the raw
 * constant pool, so we must use cp->uncached_x methods when appropriate.
 */
JVM_ENTRY(const char*, JVM_GetCPFieldNameUTF(JNIEnv *env, jclass cls, jint cp_index))
  JVMWrapper("JVM_GetCPFieldNameUTF");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  ConstantPool* cp = InstanceKlass::cast(k)->constants();
  switch (cp->tag_at(cp_index).value()) {
    case JVM_CONSTANT_Fieldref:
      return cp->uncached_name_ref_at(cp_index)->as_utf8();
    default:
      fatal("JVM_GetCPFieldNameUTF: illegal constant");
  }
  ShouldNotReachHere();
  return NULL;
JVM_END


JVM_ENTRY(const char*, JVM_GetCPMethodNameUTF(JNIEnv *env, jclass cls, jint cp_index))
  JVMWrapper("JVM_GetCPMethodNameUTF");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  ConstantPool* cp = InstanceKlass::cast(k)->constants();
  switch (cp->tag_at(cp_index).value()) {
    case JVM_CONSTANT_InterfaceMethodref:
    case JVM_CONSTANT_Methodref:
      return cp->uncached_name_ref_at(cp_index)->as_utf8();
    default:
      fatal("JVM_GetCPMethodNameUTF: illegal constant");
  }
  ShouldNotReachHere();
  return NULL;
JVM_END


JVM_ENTRY(const char*, JVM_GetCPMethodSignatureUTF(JNIEnv *env, jclass cls, jint cp_index))
  JVMWrapper("JVM_GetCPMethodSignatureUTF");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  ConstantPool* cp = InstanceKlass::cast(k)->constants();
  switch (cp->tag_at(cp_index).value()) {
    case JVM_CONSTANT_InterfaceMethodref:
    case JVM_CONSTANT_Methodref:
      return cp->uncached_signature_ref_at(cp_index)->as_utf8();
    default:
      fatal("JVM_GetCPMethodSignatureUTF: illegal constant");
  }
  ShouldNotReachHere();
  return NULL;
JVM_END


JVM_ENTRY(const char*, JVM_GetCPFieldSignatureUTF(JNIEnv *env, jclass cls, jint cp_index))
  JVMWrapper("JVM_GetCPFieldSignatureUTF");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  ConstantPool* cp = InstanceKlass::cast(k)->constants();
  switch (cp->tag_at(cp_index).value()) {
    case JVM_CONSTANT_Fieldref:
      return cp->uncached_signature_ref_at(cp_index)->as_utf8();
    default:
      fatal("JVM_GetCPFieldSignatureUTF: illegal constant");
  }
  ShouldNotReachHere();
  return NULL;
JVM_END


JVM_ENTRY(const char*, JVM_GetCPClassNameUTF(JNIEnv *env, jclass cls, jint cp_index))
  JVMWrapper("JVM_GetCPClassNameUTF");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  ConstantPool* cp = InstanceKlass::cast(k)->constants();
  Symbol* classname = cp->klass_name_at(cp_index);
  return classname->as_utf8();
JVM_END


JVM_ENTRY(const char*, JVM_GetCPFieldClassNameUTF(JNIEnv *env, jclass cls, jint cp_index))
  JVMWrapper("JVM_GetCPFieldClassNameUTF");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  ConstantPool* cp = InstanceKlass::cast(k)->constants();
  switch (cp->tag_at(cp_index).value()) {
    case JVM_CONSTANT_Fieldref: {
      int class_index = cp->uncached_klass_ref_index_at(cp_index);
      Symbol* classname = cp->klass_name_at(class_index);
      return classname->as_utf8();
    }
    default:
      fatal("JVM_GetCPFieldClassNameUTF: illegal constant");
  }
  ShouldNotReachHere();
  return NULL;
JVM_END


JVM_ENTRY(const char*, JVM_GetCPMethodClassNameUTF(JNIEnv *env, jclass cls, jint cp_index))
  JVMWrapper("JVM_GetCPMethodClassNameUTF");
  Klass* k = java_lang_Class::as_Klass(JNIHandles::resolve_non_null(cls));
  // k = JvmtiThreadState::class_to_verify_considering_redefinition(k, thread);
  ConstantPool* cp = InstanceKlass::cast(k)->constants();
  switch (cp->tag_at(cp_index).value()) {
    case JVM_CONSTANT_Methodref:
    case JVM_CONSTANT_InterfaceMethodref: {
      int class_index = cp->uncached_klass_ref_index_at(cp_index);
      Symbol* classname = cp->klass_name_at(class_index);
      return classname->as_utf8();
    }
    default:
      fatal("JVM_GetCPMethodClassNameUTF: illegal constant");
  }
  ShouldNotReachHere();
  return NULL;
JVM_END


JVM_ENTRY(jint, JVM_GetCPFieldModifiers(JNIEnv *env, jclass cls, int cp_index, jclass called_cls))
  JVMWrapper("JVM_GetCPFieldModifiers");
  D_WARN_Unimplement;
JVM_END


JVM_QUICK_ENTRY(jint, JVM_GetCPMethodModifiers(JNIEnv *env, jclass cls, int cp_index, jclass called_cls))
  JVMWrapper("JVM_GetCPMethodModifiers");
  D_WARN_Unimplement;
JVM_END


// Misc //////////////////////////////////////////////////////////////////////////////////////////////

JVM_LEAF(void, JVM_ReleaseUTF(const char *utf))
  // So long as UTF8::convert_to_utf8 returns resource strings, we don't have to do anything
JVM_END


JVM_ENTRY(jboolean, JVM_IsSameClassPackage(JNIEnv *env, jclass class1, jclass class2))
  JVMWrapper("JVM_IsSameClassPackage");
  oop class1_mirror = JNIHandles::resolve_non_null(class1);
  oop class2_mirror = JNIHandles::resolve_non_null(class2);
  Klass* klass1 = java_lang_Class::as_Klass(class1_mirror);
  Klass* klass2 = java_lang_Class::as_Klass(class2_mirror);
  return (jboolean) Reflection::is_same_class_package(klass1, klass2);
JVM_END


// IO functions ////////////////////////////////////////////////////////////////////////////////////////

JVM_LEAF(jint, JVM_Open(const char *fname, jint flags, jint mode))
  JVMWrapper2("JVM_Open (%s)", fname);

  //%note jvm_r6
  int result = os::open(fname, flags, mode);
  if (result >= 0) {
    return result;
  } else {
    switch(errno) {
      case EEXIST:
        return JVM_EEXIST;
      default:
        return -1;
    }
  }
JVM_END


JVM_LEAF(jint, JVM_Close(jint fd))
  JVMWrapper2("JVM_Close (0x%x)", fd);
  //%note jvm_r6
  return os::close(fd);
JVM_END


JVM_LEAF(jint, JVM_Read(jint fd, char *buf, jint nbytes))
  JVMWrapper2("JVM_Read (0x%x)", fd);

  //%note jvm_r6
  return (jint)os::restartable_read(fd, buf, nbytes);
JVM_END


JVM_LEAF(jint, JVM_Write(jint fd, char *buf, jint nbytes))
  JVMWrapper2("JVM_Write (0x%x)", fd);

  //%note jvm_r6
  return (jint)os::write(fd, buf, nbytes);
JVM_END


JVM_LEAF(jint, JVM_Available(jint fd, jlong *pbytes))
  JVMWrapper2("JVM_Available (0x%x)", fd);
  //%note jvm_r6
  return os::available(fd, pbytes);
JVM_END


JVM_LEAF(jlong, JVM_Lseek(jint fd, jlong offset, jint whence))
  JVMWrapper4("JVM_Lseek (0x%x, " INT64_FORMAT ", %d)", fd, (int64_t) offset, whence);
  //%note jvm_r6
  return os::lseek(fd, offset, whence);
JVM_END


JVM_LEAF(jint, JVM_SetLength(jint fd, jlong length))
  JVMWrapper3("JVM_SetLength (0x%x, " INT64_FORMAT ")", fd, (int64_t) length);
  return os::ftruncate(fd, length);
JVM_END


JVM_LEAF(jint, JVM_Sync(jint fd))
  JVMWrapper2("JVM_Sync (0x%x)", fd);
  //%note jvm_r6
  return os::fsync(fd);
JVM_END


// Printing support //////////////////////////////////////////////////
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
  return vfprintf(f, fmt, NULL);
}

ATTRIBUTE_PRINTF(1, 2)
JNIEXPORT int jio_printf(const char *fmt, ...) {
  int len;
  va_list args;
  va_start(args, fmt);
  // jianyu: write to stdout
  printf(fmt, args);
  va_end(args);
  return len;
}


// HotSpot specific jio method
void jio_print(const char* s) {
  // Make an unused local variable to avoid warning from gcc 4.x compiler.
  // jianyu: write to stdout
  size_t count = ::write(stdout, s, (int)strlen(s));
}

} // Extern C

// java.lang.Thread //////////////////////////////////////////////////////////////////////////////

// In most of the JVM Thread support functions we need to be sure to lock the Threads_lock
// to prevent the target thread from exiting after we have a pointer to the C++ Thread or
// OSThread objects.  The exception to this rule is when the target object is the thread
// doing the operation, in which case we know that the thread won't exit until the
// operation is done (all exits being voluntary).  There are a few cases where it is
// rather silly to do operations on yourself, like resuming yourself or asking whether
// you are alive.  While these can still happen, they are not subject to deadlocks if
// the lock is held while the operation occurs (this is not the case for suspend, for
// instance), and are very unlikely.  Because IsAlive needs to be fast and its
// implementation is local to this file, we always lock Threads_lock for that one.

static void thread_entry(JavaThread* thread, TRAPS) {
  HandleMark hm(THREAD);
  Handle obj(THREAD, thread->threadObj());
  JavaValue result(T_VOID);
  JavaCalls::call_virtual(&result,
                          obj,
                          KlassHandle(THREAD, SystemDictionary::Thread_klass()),
                          vmSymbols::run_method_name(),
                          vmSymbols::void_method_signature(),
                          THREAD);
}


JVM_ENTRY(void, JVM_StartThread(JNIEnv* env, jobject jthread))
  JVMWrapper("JVM_StartThread");
  D_WARN_Unimplement;
JVM_END

// JVM_Stop is implemented using a VM_Operation, so threads are forced to safepoints
// before the quasi-asynchronous exception is delivered.  This is a little obtrusive,
// but is thought to be reliable and simple. In the case, where the receiver is the
// same thread as the sender, no safepoint is needed.
JVM_ENTRY(void, JVM_StopThread(JNIEnv* env, jobject jthread, jobject throwable))
  JVMWrapper("JVM_StopThread");
  D_WARN_Unimplement;
JVM_END


JVM_ENTRY(jboolean, JVM_IsThreadAlive(JNIEnv* env, jobject jthread))
  JVMWrapper("JVM_IsThreadAlive");

  oop thread_oop = JNIHandles::resolve_non_null(jthread);
  return java_lang_Thread::is_alive(thread_oop);
JVM_END


JVM_ENTRY(void, JVM_SuspendThread(JNIEnv* env, jobject jthread))
  JVMWrapper("JVM_SuspendThread");
  D_WARN_Unimplement;
JVM_END


JVM_ENTRY(void, JVM_ResumeThread(JNIEnv* env, jobject jthread))
  JVMWrapper("JVM_ResumeThread");
  D_WARN_Unimplement;
JVM_END


JVM_ENTRY(void, JVM_SetThreadPriority(JNIEnv* env, jobject jthread, jint prio))
  JVMWrapper("JVM_SetThreadPriority");
  // Ensure that the C++ Thread and OSThread structures aren't freed before we operate
  D_WARN_Unimplement;
JVM_END


JVM_ENTRY(void, JVM_Yield(JNIEnv *env, jclass threadClass))
  JVMWrapper("JVM_Yield");
  if (os::dont_yield()) return;
  // When ConvertYieldToSleep is off (default), this matches the classic VM use of yield.
  // Critical for similar threading behaviour
  if (ConvertYieldToSleep) {
    os::sleep(thread, MinSleepInterval, false);
  } else {
    os::yield();
  }
JVM_END


JVM_ENTRY(void, JVM_Sleep(JNIEnv* env, jclass threadClass, jlong millis))
  JVMWrapper("JVM_Sleep");
  D_WARN_Unimplement;
JVM_END

JVM_ENTRY(jobject, JVM_CurrentThread(JNIEnv* env, jclass threadClass))
  JVMWrapper("JVM_CurrentThread");
  oop jthread = thread->threadObj();
  assert (thread != NULL, "no current thread!");
  return JNIHandles::make_local(env, jthread);
JVM_END


JVM_ENTRY(jint, JVM_CountStackFrames(JNIEnv* env, jobject jthread))
  JVMWrapper("JVM_CountStackFrames");

  // Ensure that the C++ Thread and OSThread structures aren't freed before we operate
  oop java_thread = JNIHandles::resolve_non_null(jthread);
  bool throw_illegal_thread_state = false;
  int count = 0;
  D_WARN_Unimplement;
  return count;
JVM_END

// Consider: A better way to implement JVM_Interrupt() is to acquire
// Threads_lock to resolve the jthread into a Thread pointer, fetch
// Thread->platformevent, Thread->native_thr, Thread->parker, etc.,
// drop Threads_lock, and the perform the unpark() and thr_kill() operations
// outside the critical section.  Threads_lock is hot so we want to minimize
// the hold-time.  A cleaner interface would be to decompose interrupt into
// two steps.  The 1st phase, performed under Threads_lock, would return
// a closure that'd be invoked after Threads_lock was dropped.
// This tactic is safe as PlatformEvent and Parkers are type-stable (TSM) and
// admit spurious wakeups.

JVM_ENTRY(void, JVM_Interrupt(JNIEnv* env, jobject jthread))
  JVMWrapper("JVM_Interrupt");
  D_WARN_Unimplement;
JVM_END


JVM_QUICK_ENTRY(jboolean, JVM_IsInterrupted(JNIEnv* env, jobject jthread, jboolean clear_interrupted))
  JVMWrapper("JVM_IsInterrupted");
  D_WARN_Unimplement;
  return false;
JVM_END


// Return true iff the current thread has locked the object passed in

JVM_ENTRY(jboolean, JVM_HoldsLock(JNIEnv* env, jclass threadClass, jobject obj))
  JVMWrapper("JVM_HoldsLock");
  D_WARN_Unimplement;
JVM_END


JVM_ENTRY(void, JVM_DumpAllStacks(JNIEnv* env, jclass))
  JVMWrapper("JVM_DumpAllStacks");
  D_WARN_Unimplement;
JVM_END

JVM_ENTRY(void, JVM_SetNativeThreadName(JNIEnv* env, jobject jthread, jstring name))
  JVMWrapper("JVM_SetNativeThreadName");
  D_WARN_Unimplement;
JVM_END

// java.lang.SecurityManager ///////////////////////////////////////////////////////////////////////

static bool is_trusted_frame(JavaThread* jthread, vframeStream* vfst) {
  assert(jthread->is_Java_thread(), "must be a Java thread");
  D_WARN_Unimplement;
  return false;
}

JVM_ENTRY(jclass, JVM_CurrentLoadedClass(JNIEnv *env))
  JVMWrapper("JVM_CurrentLoadedClass");
  ResourceMark rm(THREAD);

  for (vframeStream vfst(thread); !vfst.at_end(); vfst.next()) {
    // if a method in a class in a trusted loader is in a doPrivileged, return NULL
    bool trusted = is_trusted_frame(thread, &vfst);
    if (trusted) return NULL;

    Method* m = vfst.method();
    if (!m->is_native()) {
      InstanceKlass* holder = m->method_holder();
      oop loader = holder->class_loader();
      if (loader != NULL && !java_lang_ClassLoader::is_trusted_loader(loader)) {
        return (jclass) JNIHandles::make_local(env, holder->java_mirror());
      }
    }
  }
  return NULL;
JVM_END


JVM_ENTRY(jobject, JVM_CurrentClassLoader(JNIEnv *env))
  JVMWrapper("JVM_CurrentClassLoader");
  ResourceMark rm(THREAD);

  for (vframeStream vfst(thread); !vfst.at_end(); vfst.next()) {

    // if a method in a class in a trusted loader is in a doPrivileged, return NULL
    bool trusted = is_trusted_frame(thread, &vfst);
    if (trusted) return NULL;

    Method* m = vfst.method();
    if (!m->is_native()) {
      InstanceKlass* holder = m->method_holder();
      assert(holder->is_klass(), "just checking");
      oop loader = holder->class_loader();
      if (loader != NULL && !java_lang_ClassLoader::is_trusted_loader(loader)) {
        return JNIHandles::make_local(env, loader);
      }
    }
  }
  return NULL;
JVM_END


JVM_ENTRY(jobjectArray, JVM_GetClassContext(JNIEnv *env))
  JVMWrapper("JVM_GetClassContext");
  ResourceMark rm(THREAD);
  // JvmtiVMObjectAllocEventCollector oam;
  vframeStream vfst(thread);

  if (SystemDictionary::reflect_CallerSensitive_klass() != NULL) {
    // This must only be called from SecurityManager.getClassContext
    Method* m = vfst.method();
    if (!(m->method_holder() == SystemDictionary::SecurityManager_klass() &&
          m->name()          == vmSymbols::getClassContext_name() &&
          m->signature()     == vmSymbols::void_class_array_signature())) {
      THROW_MSG_NULL(vmSymbols::java_lang_InternalError(), "JVM_GetClassContext must only be called from SecurityManager.getClassContext");
    }
  }

  // Collect method holders
  GrowableArray<KlassHandle>* klass_array = new GrowableArray<KlassHandle>();
  for (; !vfst.at_end(); vfst.security_next()) {
    Method* m = vfst.method();
    // Native frames are not returned
    if (!m->is_ignored_by_security_stack_walk() && !m->is_native()) {
      Klass* holder = m->method_holder();
      assert(holder->is_klass(), "just checking");
      klass_array->append(holder);
    }
  }

  // Create result array of type [Ljava/lang/Class;
  objArrayOop result = oopFactory::new_objArray(SystemDictionary::Class_klass(), klass_array->length(), CHECK_NULL);
  // Fill in mirrors corresponding to method holders
  for (int i = 0; i < klass_array->length(); i++) {
    result->obj_at_put(i, klass_array->at(i)->java_mirror());
  }

  return (jobjectArray) JNIHandles::make_local(env, result);
JVM_END


JVM_ENTRY(jint, JVM_ClassDepth(JNIEnv *env, jstring name))
  JVMWrapper("JVM_ClassDepth");
  ResourceMark rm(THREAD);
  Handle h_name (THREAD, JNIHandles::resolve_non_null(name));
  Handle class_name_str = java_lang_String::internalize_classname(h_name, CHECK_0);

  const char* str = java_lang_String::as_utf8_string(class_name_str());
  TempNewSymbol class_name_sym = SymbolTable::probe(str, (int)strlen(str));
  if (class_name_sym == NULL) {
    return -1;
  }

  int depth = 0;

  for(vframeStream vfst(thread); !vfst.at_end(); vfst.next()) {
    if (!vfst.method()->is_native()) {
      InstanceKlass* holder = vfst.method()->method_holder();
      assert(holder->is_klass(), "just checking");
      if (holder->name() == class_name_sym) {
        return depth;
      }
      depth++;
    }
  }
  return -1;
JVM_END


JVM_ENTRY(jint, JVM_ClassLoaderDepth(JNIEnv *env))
  JVMWrapper("JVM_ClassLoaderDepth");
  ResourceMark rm(THREAD);
  int depth = 0;
  for (vframeStream vfst(thread); !vfst.at_end(); vfst.next()) {
    // if a method in a class in a trusted loader is in a doPrivileged, return -1
    bool trusted = is_trusted_frame(thread, &vfst);
    if (trusted) return -1;

    Method* m = vfst.method();
    if (!m->is_native()) {
      InstanceKlass* holder = m->method_holder();
      assert(holder->is_klass(), "just checking");
      oop loader = holder->class_loader();
      if (loader != NULL && !java_lang_ClassLoader::is_trusted_loader(loader)) {
        return depth;
      }
      depth++;
    }
  }
  return -1;
JVM_END


// java.lang.Package ////////////////////////////////////////////////////////////////


JVM_ENTRY(jstring, JVM_GetSystemPackage(JNIEnv *env, jstring name))
  JVMWrapper("JVM_GetSystemPackage");
  ResourceMark rm(THREAD);
  // JvmtiVMObjectAllocEventCollector oam;
  char* str = java_lang_String::as_utf8_string(JNIHandles::resolve_non_null(name));
  oop result = ClassLoader::get_system_package(str, CHECK_NULL);
  return (jstring) JNIHandles::make_local(result);
JVM_END


JVM_ENTRY(jobjectArray, JVM_GetSystemPackages(JNIEnv *env))
  JVMWrapper("JVM_GetSystemPackages");
  // JvmtiVMObjectAllocEventCollector oam;
  objArrayOop result = ClassLoader::get_system_packages(CHECK_NULL);
  return (jobjectArray) JNIHandles::make_local(result);
JVM_END


// ObjectInputStream ///////////////////////////////////////////////////////////////

bool force_verify_field_access(Klass* current_class, Klass* field_class, AccessFlags access, bool classloader_only) {
  if (current_class == NULL) {
    return true;
  }
  if ((current_class == field_class) || access.is_public()) {
    return true;
  }

  if (access.is_protected()) {
    // See if current_class is a subclass of field_class
    if (current_class->is_subclass_of(field_class)) {
      return true;
    }
  }

  return (!access.is_private() && InstanceKlass::cast(current_class)->is_same_class_package(field_class));
}


// JVM_AllocateNewObject and JVM_AllocateNewArray are unused as of 1.4
JVM_ENTRY(jobject, JVM_AllocateNewObject(JNIEnv *env, jobject receiver, jclass currClass, jclass initClass))
  JVMWrapper("JVM_AllocateNewObject");
  // JvmtiVMObjectAllocEventCollector oam;
  // Receiver is not used
  oop curr_mirror = JNIHandles::resolve_non_null(currClass);
  oop init_mirror = JNIHandles::resolve_non_null(initClass);

  // Cannot instantiate primitive types
  if (java_lang_Class::is_primitive(curr_mirror) || java_lang_Class::is_primitive(init_mirror)) {
    ResourceMark rm(THREAD);
    THROW_0(vmSymbols::java_lang_InvalidClassException());
  }

  // Arrays not allowed here, must use JVM_AllocateNewArray
  if (java_lang_Class::as_Klass(curr_mirror)->oop_is_array() ||
      java_lang_Class::as_Klass(init_mirror)->oop_is_array()) {
    ResourceMark rm(THREAD);
    THROW_0(vmSymbols::java_lang_InvalidClassException());
  }

  instanceKlassHandle curr_klass (THREAD, java_lang_Class::as_Klass(curr_mirror));
  instanceKlassHandle init_klass (THREAD, java_lang_Class::as_Klass(init_mirror));

  assert(curr_klass->is_subclass_of(init_klass()), "just checking");

  // Interfaces, abstract classes, and java.lang.Class classes cannot be instantiated directly.
  curr_klass->check_valid_for_instantiation(false, CHECK_NULL);

  // Make sure klass is initialized, since we are about to instantiate one of them.
  curr_klass->initialize(CHECK_NULL);

  methodHandle m (THREAD,
                  init_klass->find_method(vmSymbols::object_initializer_name(),
                                          vmSymbols::void_method_signature()));
  if (m.is_null()) {
    ResourceMark rm(THREAD);
    THROW_MSG_0(vmSymbols::java_lang_NoSuchMethodError(),
                Method::name_and_sig_as_C_string(init_klass(),
                                                 vmSymbols::object_initializer_name(),
                                                 vmSymbols::void_method_signature()));
  }

  if (curr_klass ==  init_klass && !m->is_public()) {
    // Calling the constructor for class 'curr_klass'.
    // Only allow calls to a public no-arg constructor.
    // This path corresponds to creating an Externalizable object.
    THROW_0(vmSymbols::java_lang_IllegalAccessException());
  }

  if (!force_verify_field_access(curr_klass(), init_klass(), m->access_flags(), false)) {
    // subclass 'curr_klass' does not have access to no-arg constructor of 'initcb'
    THROW_0(vmSymbols::java_lang_IllegalAccessException());
  }

  Handle obj = curr_klass->allocate_instance_handle(CHECK_NULL);
  // Call constructor m. This might call a constructor higher up in the hierachy
  JavaCalls::call_default_constructor(thread, m, obj, CHECK_NULL);

  return JNIHandles::make_local(obj());
JVM_END


JVM_ENTRY(jobject, JVM_AllocateNewArray(JNIEnv *env, jobject obj, jclass currClass, jint length))
  JVMWrapper("JVM_AllocateNewArray");
  // JvmtiVMObjectAllocEventCollector oam;
  oop mirror = JNIHandles::resolve_non_null(currClass);

  if (java_lang_Class::is_primitive(mirror)) {
    THROW_0(vmSymbols::java_lang_InvalidClassException());
  }
  Klass* k = java_lang_Class::as_Klass(mirror);
  oop result;

  if (k->oop_is_typeArray()) {
    // typeArray
    result = TypeArrayKlass::cast(k)->allocate(length, CHECK_NULL);
  } else if (k->oop_is_objArray()) {
    // objArray
    ObjArrayKlass* oak = ObjArrayKlass::cast(k);
    oak->initialize(CHECK_NULL); // make sure class is initialized (matches Classic VM behavior)
    result = oak->allocate(length, CHECK_NULL);
  } else {
    THROW_0(vmSymbols::java_lang_InvalidClassException());
  }
  return JNIHandles::make_local(env, result);
JVM_END


// Return the first non-null class loader up the execution stack, or null
// if only code from the null class loader is on the stack.

JVM_ENTRY(jobject, JVM_LatestUserDefinedLoader(JNIEnv *env))
  for (vframeStream vfst(thread); !vfst.at_end(); vfst.next()) {
    // UseNewReflection
    vfst.skip_reflection_related_frames(); // Only needed for 1.4 reflection
    oop loader = vfst.method()->method_holder()->class_loader();
    if (loader != NULL) {
      return JNIHandles::make_local(env, loader);
    }
  }
  return NULL;
JVM_END


// Load a class relative to the most recent class on the stack  with a non-null
// classloader.
// This function has been deprecated and should not be considered part of the
// specified JVM interface.

JVM_ENTRY(jclass, JVM_LoadClass0(JNIEnv *env, jobject receiver,
        jclass currClass, jstring currClassName))
  JVMWrapper("JVM_LoadClass0");
  // Receiver is not used
  ResourceMark rm(THREAD);

  // Class name argument is not guaranteed to be in internal format
  Handle classname (THREAD, JNIHandles::resolve_non_null(currClassName));
  Handle string = java_lang_String::internalize_classname(classname, CHECK_NULL);

  const char* str = java_lang_String::as_utf8_string(string());

  if (str == NULL || (int)strlen(str) > Symbol::max_length()) {
    // It's impossible to create this class;  the name cannot fit
    // into the constant pool.
    THROW_MSG_0(vmSymbols::java_lang_NoClassDefFoundError(), str);
  }

  TempNewSymbol name = SymbolTable::new_symbol(str, CHECK_NULL);
  Handle curr_klass (THREAD, JNIHandles::resolve(currClass));
  // Find the most recent class on the stack with a non-null classloader
  oop loader = NULL;
  oop protection_domain = NULL;
  if (curr_klass.is_null()) {
    for (vframeStream vfst(thread);
         !vfst.at_end() && loader == NULL;
         vfst.next()) {
      if (!vfst.method()->is_native()) {
        InstanceKlass* holder = vfst.method()->method_holder();
        loader             = holder->class_loader();
        protection_domain  = holder->protection_domain();
      }
    }
  } else {
    Klass* curr_klass_oop = java_lang_Class::as_Klass(curr_klass());
    loader            = InstanceKlass::cast(curr_klass_oop)->class_loader();
    protection_domain = InstanceKlass::cast(curr_klass_oop)->protection_domain();
  }
  Handle h_loader(THREAD, loader);
  Handle h_prot  (THREAD, protection_domain);
  jclass result =  find_class_from_class_loader(env, name, true, h_loader, h_prot,
                                                false, thread);
  if (TraceClassResolution && result != NULL) {
    trace_class_resolution(java_lang_Class::as_Klass(JNIHandles::resolve_non_null(result)));
  }
  return result;
JVM_END


// Array ///////////////////////////////////////////////////////////////////////////////////////////


// resolve array handle and check arguments
static inline arrayOop check_array(JNIEnv *env, jobject arr, bool type_array_only, TRAPS) {
  if (arr == NULL) {
    THROW_0(vmSymbols::java_lang_NullPointerException());
  }
  oop a = JNIHandles::resolve_non_null(arr);
  if (!a->is_array() || (type_array_only && !a->is_typeArray())) {
    THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(), "Argument is not an array");
  }
  return arrayOop(a);
}


JVM_ENTRY(jint, JVM_GetArrayLength(JNIEnv *env, jobject arr))
  JVMWrapper("JVM_GetArrayLength");
  arrayOop a = check_array(env, arr, false, CHECK_0);
  return a->length();
JVM_END


JVM_ENTRY(jobject, JVM_GetArrayElement(JNIEnv *env, jobject arr, jint index))
  JVMWrapper("JVM_Array_Get");
  // JvmtiVMObjectAllocEventCollector oam;
  arrayOop a = check_array(env, arr, false, CHECK_NULL);
  jvalue value;
  BasicType type = Reflection::array_get(&value, a, index, CHECK_NULL);
  oop box = Reflection::box(&value, type, CHECK_NULL);
  return JNIHandles::make_local(env, box);
JVM_END


JVM_ENTRY(jvalue, JVM_GetPrimitiveArrayElement(JNIEnv *env, jobject arr, jint index, jint wCode))
  JVMWrapper("JVM_GetPrimitiveArrayElement");
  jvalue value;
  value.i = 0; // to initialize value before getting used in CHECK
  arrayOop a = check_array(env, arr, true, CHECK_(value));
  assert(a->is_typeArray(), "just checking");
  BasicType type = Reflection::array_get(&value, a, index, CHECK_(value));
  BasicType wide_type = (BasicType) wCode;
  if (type != wide_type) {
    Reflection::widen(&value, type, wide_type, CHECK_(value));
  }
  return value;
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
    value_type = Reflection::unbox_for_regular_object(box, &value);
  } else {
    value_type = Reflection::unbox_for_primitive(box, &value, CHECK);
  }
  Reflection::array_set(&value, a, index, value_type, CHECK);
JVM_END

JVM_ENTRY(void, JVM_SetArrayElement(JNIEnv *env, jobject arr, jint index, jobject val))
  JVMWrapper("JVM_SetArrayElement");
  arrayOop a = check_array(env, arr, false, CHECK);
  oop box = JNIHandles::resolve(val);
  jvalue value;
  value.i = 0; // to initialize value before getting used in CHECK
  BasicType value_type;
  if (a->is_objArray()) {
    // Make sure we do no unbox e.g. java/lang/Integer instances when storing into an object array
    value_type = Reflection::unbox_for_regular_object(box, &value);
  } else {
    value_type = Reflection::unbox_for_primitive(box, &value, CHECK);
  }
  Reflection::array_set(&value, a, index, value_type, CHECK);
JVM_END


JVM_ENTRY(void, JVM_SetPrimitiveArrayElement(JNIEnv *env, jobject arr, jint index, jvalue v, unsigned char vCode))
  JVMWrapper("JVM_SetPrimitiveArrayElement");
  arrayOop a = check_array(env, arr, true, CHECK);
  assert(a->is_typeArray(), "just checking");
  BasicType value_type = (BasicType) vCode;
  Reflection::array_set(&v, a, index, value_type, CHECK);
JVM_END

JVM_ENTRY(jobject, JVM_EnclaveNewArray(JNIEnv *env, jclass mirror, jclass eltClass, jint length))
  JVMWrapper("JVM_EnclaveNewArray");
#ifdef ENCLAVE_UNIX
  oop element_mirror = JNIHandles::resolve(eltClass);
  address ar = EnclaveMemory::static_klass_new_array((JavaThread *) THREAD, element_mirror, length);
  return JNIHandles::make_local(env, (oop)ar);
#endif
JVM_END

JVM_ENTRY(jobject, JVM_EnclaveNewMultiArray(JNIEnv *env, jclass mirror,jclass eltClass, jintArray dim))
  JVMWrapper("JVM_NewMultiArray");
#ifdef ENCLAVE_UNIX
  arrayOop dim_array = check_array(env, dim, true, CHECK_NULL);
  oop element_mirror = JNIHandles::resolve(eltClass);
  oop result = (oop) EnclaveMemory::static_klass_multi_array((JavaThread *) THREAD, element_mirror, dim_array);
  return JNIHandles::make_local(env, result);
#endif
JVM_END

JVM_ENTRY(jobject, JVM_NewArray(JNIEnv *env, jclass eltClass, jint length))
  JVMWrapper("JVM_NewArray");
#ifdef ENCLAVE_UNIX
  oop element_mirror = JNIHandles::resolve(eltClass);
  address ar = EnclaveMemory::static_klass_new_array((JavaThread *) THREAD, element_mirror, length);
  return JNIHandles::make_local(env, (oop)ar);
#else
  // JvmtiVMObjectAllocEventCollector oam;
  oop element_mirror = JNIHandles::resolve(eltClass);
  oop result = Reflection::reflect_new_array(element_mirror, length, CHECK_NULL);
  return JNIHandles::make_local(env, result);
#endif
JVM_END


JVM_ENTRY(jobject, JVM_NewMultiArray(JNIEnv *env, jclass eltClass, jintArray dim))
  JVMWrapper("JVM_NewMultiArray");
#ifdef ENCLAVE_UNIX
  arrayOop dim_array = check_array(env, dim, true, CHECK_NULL);
  oop element_mirror = JNIHandles::resolve(eltClass);
  oop result = (oop) EnclaveMemory::static_klass_multi_array((JavaThread *) THREAD, element_mirror, dim_array);
  return JNIHandles::make_local(env, result);
#else
  // JvmtiVMObjectAllocEventCollector oam;
  arrayOop dim_array = check_array(env, dim, true, CHECK_NULL);
  oop element_mirror = JNIHandles::resolve(eltClass);
  assert(dim_array->is_typeArray(), "just checking");
  oop result = Reflection::reflect_new_multi_array(element_mirror, typeArrayOop(dim_array), CHECK_NULL);
  return JNIHandles::make_local(env, result);
#endif
JVM_END


// Networking library support ////////////////////////////////////////////////////////////////////

JVM_LEAF(jint, JVM_InitializeSocketLibrary())
  JVMWrapper("JVM_InitializeSocketLibrary");
  return 0;
JVM_END


JVM_LEAF(jint, JVM_Socket(jint domain, jint type, jint protocol))
  JVMWrapper("JVM_Socket");
  return os::socket(domain, type, protocol);
JVM_END


JVM_LEAF(jint, JVM_SocketClose(jint fd))
  JVMWrapper2("JVM_SocketClose (0x%x)", fd);
  //%note jvm_r6
  return os::socket_close(fd);
JVM_END


JVM_LEAF(jint, JVM_SocketShutdown(jint fd, jint howto))
  JVMWrapper2("JVM_SocketShutdown (0x%x)", fd);
  //%note jvm_r6
  return os::socket_shutdown(fd, howto);
JVM_END


JVM_LEAF(jint, JVM_Recv(jint fd, char *buf, jint nBytes, jint flags))
  JVMWrapper2("JVM_Recv (0x%x)", fd);
  //%note jvm_r6
  return os::recv(fd, buf, (size_t)nBytes, (uint)flags);
JVM_END


JVM_LEAF(jint, JVM_Send(jint fd, char *buf, jint nBytes, jint flags))
  JVMWrapper2("JVM_Send (0x%x)", fd);
  //%note jvm_r6
  return os::send(fd, buf, (size_t)nBytes, (uint)flags);
JVM_END


JVM_LEAF(jint, JVM_Timeout(int fd, long timeout))
  JVMWrapper2("JVM_Timeout (0x%x)", fd);
  //%note jvm_r6
  return os::timeout(fd, timeout);
JVM_END


JVM_LEAF(jint, JVM_Listen(jint fd, jint count))
  JVMWrapper2("JVM_Listen (0x%x)", fd);
  //%note jvm_r6
  return os::listen(fd, count);
JVM_END


JVM_LEAF(jint, JVM_Connect(jint fd, struct sockaddr *him, jint len))
  JVMWrapper2("JVM_Connect (0x%x)", fd);
  //%note jvm_r6
  return os::connect(fd, him, (socklen_t)len);
JVM_END


JVM_LEAF(jint, JVM_Bind(jint fd, struct sockaddr *him, jint len))
  JVMWrapper2("JVM_Bind (0x%x)", fd);
  //%note jvm_r6
  return os::bind(fd, him, (socklen_t)len);
JVM_END


JVM_LEAF(jint, JVM_Accept(jint fd, struct sockaddr *him, jint *len))
  JVMWrapper2("JVM_Accept (0x%x)", fd);
  //%note jvm_r6
  socklen_t socklen = (socklen_t)(*len);
  jint result = os::accept(fd, him, &socklen);
  *len = (jint)socklen;
  return result;
JVM_END


JVM_LEAF(jint, JVM_RecvFrom(jint fd, char *buf, int nBytes, int flags, struct sockaddr *from, int *fromlen))
  JVMWrapper2("JVM_RecvFrom (0x%x)", fd);
  //%note jvm_r6
  socklen_t socklen = (socklen_t)(*fromlen);
  jint result = os::recvfrom(fd, buf, (size_t)nBytes, (uint)flags, from, &socklen);
  *fromlen = (int)socklen;
  return result;
JVM_END


JVM_LEAF(jint, JVM_GetSockName(jint fd, struct sockaddr *him, int *len))
  JVMWrapper2("JVM_GetSockName (0x%x)", fd);
  //%note jvm_r6
  socklen_t socklen = (socklen_t)(*len);
  jint result = os::get_sock_name(fd, him, &socklen);
  *len = (int)socklen;
  return result;
JVM_END


JVM_LEAF(jint, JVM_SendTo(jint fd, char *buf, int len, int flags, struct sockaddr *to, int tolen))
  JVMWrapper2("JVM_SendTo (0x%x)", fd);
  //%note jvm_r6
  return os::sendto(fd, buf, (size_t)len, (uint)flags, to, (socklen_t)tolen);
JVM_END


JVM_LEAF(jint, JVM_SocketAvailable(jint fd, jint *pbytes))
  JVMWrapper2("JVM_SocketAvailable (0x%x)", fd);
  //%note jvm_r6
  return os::socket_available(fd, pbytes);
JVM_END


JVM_LEAF(jint, JVM_GetSockOpt(jint fd, int level, int optname, char *optval, int *optlen))
  JVMWrapper2("JVM_GetSockOpt (0x%x)", fd);
  //%note jvm_r6
  socklen_t socklen = (socklen_t)(*optlen);
  jint result = os::get_sock_opt(fd, level, optname, optval, &socklen);
  *optlen = (int)socklen;
  return result;
JVM_END


JVM_LEAF(jint, JVM_SetSockOpt(jint fd, int level, int optname, const char *optval, int optlen))
  JVMWrapper2("JVM_GetSockOpt (0x%x)", fd);
  //%note jvm_r6
  return os::set_sock_opt(fd, level, optname, optval, (socklen_t)optlen);
JVM_END


JVM_LEAF(int, JVM_GetHostName(char* name, int namelen))
  JVMWrapper("JVM_GetHostName");
  return os::get_host_name(name, namelen);
JVM_END


// Library support ///////////////////////////////////////////////////////////////////////////

JVM_ENTRY_NO_ENV(void*, JVM_LoadLibrary(const char* name))
  //%note jvm_ct
  JVMWrapper2("JVM_LoadLibrary (%s)", name);
  char ebuf[1024];
  void *load_result;
  {
    ThreadToNativeFromVM ttnfvm(thread);
    load_result = os::dll_load(name, ebuf, sizeof ebuf);
  }
  if (load_result == NULL) {
    char msg[1024];
    jio_snprintf(msg, sizeof msg, "%s: %s", name, ebuf);
    // Since 'ebuf' may contain a string encoded using
    // platform encoding scheme, we need to pass
    // Exceptions::unsafe_to_utf8 to the new_exception method
    // as the last argument. See bug 6367357.
    Handle h_exception =
            Exceptions::new_exception(thread,
                                      vmSymbols::java_lang_UnsatisfiedLinkError(),
                                      msg, Exceptions::unsafe_to_utf8);

    THROW_HANDLE_0(h_exception);
  }
  return load_result;
JVM_END


JVM_LEAF(void, JVM_UnloadLibrary(void* handle))
  JVMWrapper("JVM_UnloadLibrary");
  os::dll_unload(handle);
JVM_END


JVM_LEAF(void*, JVM_FindLibraryEntry(void* handle, const char* name))
  JVMWrapper2("JVM_FindLibraryEntry (%s)", name);
  return os::dll_lookup(handle, name);
JVM_END


// Floating point support ////////////////////////////////////////////////////////////////////

JVM_LEAF(jboolean, JVM_IsNaN(jdouble a))
  JVMWrapper("JVM_IsNaN");
  return g_isnan(a);
JVM_END


// JNI version ///////////////////////////////////////////////////////////////////////////////

JVM_LEAF(jboolean, JVM_IsSupportedJNIVersion(jint version))
  JVMWrapper2("JVM_IsSupportedJNIVersion (%d)", version);
  D_WARN_Unimplement;
  return false;
JVM_END


// String support ///////////////////////////////////////////////////////////////////////////

JVM_ENTRY(jstring, JVM_InternString(JNIEnv *env, jstring str))
  JVMWrapper("JVM_InternString");
  // JvmtiVMObjectAllocEventCollector oam;
  if (str == NULL) return NULL;
  oop string = JNIHandles::resolve_non_null(str);
  oop result = StringTable::intern(string, CHECK_NULL);
  return (jstring) JNIHandles::make_local(env, result);
JVM_END


// Raw monitor support //////////////////////////////////////////////////////////////////////

// The lock routine below calls lock_without_safepoint_check in order to get a raw lock
// without interfering with the safepoint mechanism. The routines are not JVM_LEAF because
// they might be called by non-java threads. The JVM_LEAF installs a NoHandleMark check
// that only works with java threads.


JNIEXPORT void* JNICALL JVM_RawMonitorCreate(void) {
  D_WARN_Unimplement;
}


JNIEXPORT void JNICALL  JVM_RawMonitorDestroy(void *mon) {
  D_WARN_Unimplement;
}


JNIEXPORT jint JNICALL JVM_RawMonitorEnter(void *mon) {
  D_WARN_Unimplement;
}


JNIEXPORT void JNICALL JVM_RawMonitorExit(void *mon) {
  D_WARN_Unimplement;
}


// Support for Serialization

typedef jfloat  (JNICALL *IntBitsToFloatFn  )(JNIEnv* env, jclass cb, jint    value);
typedef jdouble (JNICALL *LongBitsToDoubleFn)(JNIEnv* env, jclass cb, jlong   value);
typedef jint    (JNICALL *FloatToIntBitsFn  )(JNIEnv* env, jclass cb, jfloat  value);
typedef jlong   (JNICALL *DoubleToLongBitsFn)(JNIEnv* env, jclass cb, jdouble value);

static IntBitsToFloatFn   int_bits_to_float_fn   = NULL;
static LongBitsToDoubleFn long_bits_to_double_fn = NULL;
static FloatToIntBitsFn   float_to_int_bits_fn   = NULL;
static DoubleToLongBitsFn double_to_long_bits_fn = NULL;


void initialize_converter_functions() {
  if (JDK_Version::is_gte_jdk14x_version()) {
    // These functions only exist for compatibility with 1.3.1 and earlier
    return;
  }

  // called from universe_post_init()
  assert(
          int_bits_to_float_fn   == NULL &&
          long_bits_to_double_fn == NULL &&
          float_to_int_bits_fn   == NULL &&
          double_to_long_bits_fn == NULL ,
          "initialization done twice"
  );
  // initialize
  int_bits_to_float_fn   = CAST_TO_FN_PTR(IntBitsToFloatFn  , NativeLookup::base_library_lookup("java/lang/Float" , "intBitsToFloat"  , "(I)F"));
  long_bits_to_double_fn = CAST_TO_FN_PTR(LongBitsToDoubleFn, NativeLookup::base_library_lookup("java/lang/Double", "longBitsToDouble", "(J)D"));
  float_to_int_bits_fn   = CAST_TO_FN_PTR(FloatToIntBitsFn  , NativeLookup::base_library_lookup("java/lang/Float" , "floatToIntBits"  , "(F)I"));
  double_to_long_bits_fn = CAST_TO_FN_PTR(DoubleToLongBitsFn, NativeLookup::base_library_lookup("java/lang/Double", "doubleToLongBits", "(D)J"));
  // verify
  assert(
          int_bits_to_float_fn   != NULL &&
          long_bits_to_double_fn != NULL &&
          float_to_int_bits_fn   != NULL &&
          double_to_long_bits_fn != NULL ,
          "initialization failed"
  );
}



// Shared JNI/JVM entry points //////////////////////////////////////////////////////////////

jclass find_class_from_class_loader(JNIEnv* env, Symbol* name, jboolean init,
                                    Handle loader, Handle protection_domain,
                                    jboolean throwError, TRAPS) {
  // Security Note:
  //   The Java level wrapper will perform the necessary security check allowing
  //   us to pass the NULL as the initiating class loader.  The VM is responsible for
  //   the checkPackageAccess relative to the initiating class loader via the
  //   protection_domain. The protection_domain is passed as NULL by the java code
  //   if there is no security manager in 3-arg Class.forName().
  Klass* klass = SystemDictionary::resolve_or_fail(name, loader, protection_domain, throwError != 0, CHECK_NULL);

  KlassHandle klass_handle(THREAD, klass);
  // Check if we should initialize the class
  if (init && klass_handle->oop_is_instance()) {
    klass_handle->initialize(CHECK_NULL);
  }
  return (jclass) JNIHandles::make_local(env, klass_handle->java_mirror());
}


// Internal SQE debugging support ///////////////////////////////////////////////////////////

#ifndef PRODUCT

extern "C" {
  JNIEXPORT jboolean JNICALL JVM_AccessVMBooleanFlag(const char* name, jboolean* value, jboolean is_get);
  JNIEXPORT jboolean JNICALL JVM_AccessVMIntFlag(const char* name, jint* value, jboolean is_get);
  JNIEXPORT void JNICALL JVM_VMBreakPoint(JNIEnv *env, jobject obj);
}

JVM_LEAF(jboolean, JVM_AccessVMBooleanFlag(const char* name, jboolean* value, jboolean is_get))
  JVMWrapper("JVM_AccessBoolVMFlag");
  return is_get ? CommandLineFlags::boolAt((char*) name, (bool*) value) : CommandLineFlags::boolAtPut((char*) name, (bool*) value, Flag::INTERNAL);
JVM_END

JVM_LEAF(jboolean, JVM_AccessVMIntFlag(const char* name, jint* value, jboolean is_get))
  JVMWrapper("JVM_AccessVMIntFlag");
  intx v;
  jboolean result = is_get ? CommandLineFlags::intxAt((char*) name, &v) : CommandLineFlags::intxAtPut((char*) name, &v, Flag::INTERNAL);
  *value = (jint)v;
  return result;
JVM_END


JVM_ENTRY(void, JVM_VMBreakPoint(JNIEnv *env, jobject obj))
  JVMWrapper("JVM_VMBreakPoint");
  oop the_obj = JNIHandles::resolve(obj);
  BREAKPOINT;
JVM_END


#endif


// Method ///////////////////////////////////////////////////////////////////////////////////////////

JVM_ENTRY(jobject, JVM_InvokeMethod(JNIEnv *env, jobject method, jobject obj, jobjectArray args0))
  JVMWrapper("JVM_InvokeMethod");
  Handle method_handle;
  bool stack_overflow = false;
  if (!stack_overflow) {
    method_handle = Handle(THREAD, JNIHandles::resolve(method));
    Handle receiver(THREAD, JNIHandles::resolve(obj));
    objArrayHandle args(THREAD, objArrayOop(JNIHandles::resolve(args0)));
    oop result = Reflection::invoke_method(method_handle(), receiver, args, CHECK_NULL);
    jobject res = JNIHandles::make_local(env, result);
    return res;
  } else {
    THROW_0(vmSymbols::java_lang_StackOverflowError());
  }
JVM_END


JVM_ENTRY(jobject, JVM_NewInstanceFromConstructor(JNIEnv *env, jobject c, jobjectArray args0))
  JVMWrapper("JVM_NewInstanceFromConstructor");
  oop constructor_mirror = JNIHandles::resolve(c);
  objArrayHandle args(THREAD, objArrayOop(JNIHandles::resolve(args0)));
  oop result = Reflection::invoke_constructor(constructor_mirror, args, CHECK_NULL);
  jobject res = JNIHandles::make_local(env, result);
  return res;
JVM_END

// Atomic ///////////////////////////////////////////////////////////////////////////////////////////

JVM_LEAF(jboolean, JVM_SupportsCX8())
  JVMWrapper("JVM_SupportsCX8");
  return VM_Version::supports_cx8();
JVM_END


JVM_ENTRY(jboolean, JVM_CX8Field(JNIEnv *env, jobject obj, jfieldID fid, jlong oldVal, jlong newVal))
  JVMWrapper("JVM_CX8Field");
  jlong res;
  oop             o       = JNIHandles::resolve(obj);
  intptr_t        fldOffs = jfieldIDWorkaround::from_instance_jfieldID(o->klass(), fid);
  volatile jlong* addr    = (volatile jlong*)((address)o + fldOffs);

  assert(VM_Version::supports_cx8(), "cx8 not supported");
  res = Atomic::cmpxchg(newVal, addr, oldVal);

  return res == oldVal;
JVM_END

// DTrace ///////////////////////////////////////////////////////////////////

JVM_ENTRY(jint, JVM_DTraceGetVersion(JNIEnv* env))
  JVMWrapper("JVM_DTraceGetVersion");
  return (jint)JVM_TRACING_DTRACE_VERSION;
JVM_END

JVM_ENTRY(jlong,JVM_DTraceActivate(
        JNIEnv* env, jint version, jstring module_name, jint providers_count,
        JVM_DTraceProvider* providers))
  JVMWrapper("JVM_DTraceActivate");
  D_WARN_Unimplement;
JVM_END

JVM_ENTRY(jboolean,JVM_DTraceIsProbeEnabled(JNIEnv* env, jmethodID method))
  JVMWrapper("JVM_DTraceIsProbeEnabled");
  D_WARN_Unimplement;
JVM_END

JVM_ENTRY(void,JVM_DTraceDispose(JNIEnv* env, jlong handle))
  JVMWrapper("JVM_DTraceDispose");
  D_WARN_Unimplement;
JVM_END

JVM_ENTRY(jboolean,JVM_DTraceIsSupported(JNIEnv* env))
  JVMWrapper("JVM_DTraceIsSupported");
  D_WARN_Unimplement;
JVM_END

// Returns an array of all live Thread objects (VM internal JavaThreads,
// jvmti agent threads, and JNI attaching threads  are skipped)
// See CR 6404306 regarding JNI attaching threads
JVM_ENTRY(jobjectArray, JVM_GetAllThreads(JNIEnv *env, jclass dummy))
  ResourceMark rm(THREAD);
//  ThreadsListEnumerator tle(THREAD, false, false);
  // JvmtiVMObjectAllocEventCollector oam;

  D_WARN_Unimplement;
JVM_END


// Support for java.lang.Thread.getStackTrace() and getAllStackTraces() methods
// Return StackTraceElement[][], each element is the stack trace of a thread in
// the corresponding entry in the given threads array
JVM_ENTRY(jobjectArray, JVM_DumpThreads(JNIEnv *env, jclass threadClass, jobjectArray threads))
  JVMWrapper("JVM_DumpThreads");
  D_WARN_Unimplement;
JVM_END

// JVM monitoring and management support
JVM_ENTRY_NO_ENV(void*, JVM_GetManagement(jint version))
  D_WARN_Unimplement;
JVM_END

// com.sun.tools.attach.VirtualMachine agent properties support
//
// Initialize the agent properties with the properties maintained in the VM
JVM_ENTRY(jobject, JVM_InitAgentProperties(JNIEnv *env, jobject properties))
  JVMWrapper("JVM_InitAgentProperties");
  D_WARN_Unimplement;
JVM_END

JVM_ENTRY(jobjectArray, JVM_GetEnclosingMethodInfo(JNIEnv *env, jclass ofClass))
  {
    JVMWrapper("JVM_GetEnclosingMethodInfo");
    // JvmtiVMObjectAllocEventCollector oam;

    if (ofClass == NULL) {
      return NULL;
    }
    Handle mirror(THREAD, JNIHandles::resolve_non_null(ofClass));
    // Special handling for primitive objects
    if (java_lang_Class::is_primitive(mirror())) {
      return NULL;
    }
    Klass* k = java_lang_Class::as_Klass(mirror());
    if (!k->oop_is_instance()) {
      return NULL;
    }
    instanceKlassHandle ik_h(THREAD, k);
    int encl_method_class_idx = ik_h->enclosing_method_class_index();
    if (encl_method_class_idx == 0) {
      return NULL;
    }
    objArrayOop dest_o = oopFactory::new_objArray(SystemDictionary::Object_klass(), 3, CHECK_NULL);
    objArrayHandle dest(THREAD, dest_o);
    Klass* enc_k = ik_h->constants()->klass_at(encl_method_class_idx, CHECK_NULL);
    dest->obj_at_put(0, enc_k->java_mirror());
    int encl_method_method_idx = ik_h->enclosing_method_method_index();
    if (encl_method_method_idx != 0) {
      Symbol* sym = ik_h->constants()->symbol_at(
              extract_low_short_from_int(
                      ik_h->constants()->name_and_type_at(encl_method_method_idx)));
      Handle str = java_lang_String::create_from_symbol(sym, CHECK_NULL);
      dest->obj_at_put(1, str());
      sym = ik_h->constants()->symbol_at(
              extract_high_short_from_int(
                      ik_h->constants()->name_and_type_at(encl_method_method_idx)));
      str = java_lang_String::create_from_symbol(sym, CHECK_NULL);
      dest->obj_at_put(2, str());
    }
    return (jobjectArray) JNIHandles::make_local(dest());
  }
JVM_END

JVM_ENTRY(jintArray, JVM_GetThreadStateValues(JNIEnv* env,
                                              jint javaThreadState))
  D_WARN_Unimplement;
JVM_END


JVM_ENTRY(jobjectArray, JVM_GetThreadStateNames(JNIEnv* env,
                                                jint javaThreadState,
        jintArray values))
  {
    // If new thread states are added in future JDK and VM versions,
    // this should check if the JDK version is compatible with thread
    // states supported by the VM.  Return NULL if not compatible.
    //
    // This function must map the VM java_lang_Thread::ThreadStatus
    // to the Java thread state that the JDK supports.
    //

    ResourceMark rm;

    // Check if threads is null
    if (values == NULL) {
      THROW_(vmSymbols::java_lang_NullPointerException(), 0);
    }

    typeArrayOop v = typeArrayOop(JNIHandles::resolve_non_null(values));
    typeArrayHandle values_h(THREAD, v);

    objArrayHandle names_h;
    switch (javaThreadState) {
      case JAVA_THREAD_STATE_NEW : {
        assert(values_h->length() == 1 &&
               values_h->int_at(0) == java_lang_Thread::NEW,
               "Invalid threadStatus value");

        objArrayOop r = oopFactory::new_objArray(SystemDictionary::String_klass(),
                                                 1, /* only 1 substate */
        CHECK_NULL);
        names_h = objArrayHandle(THREAD, r);
        Handle name = java_lang_String::create_from_str("NEW", CHECK_NULL);
        names_h->obj_at_put(0, name());
        break;
      }
      case JAVA_THREAD_STATE_RUNNABLE : {
        assert(values_h->length() == 1 &&
               values_h->int_at(0) == java_lang_Thread::RUNNABLE,
               "Invalid threadStatus value");

        objArrayOop r = oopFactory::new_objArray(SystemDictionary::String_klass(),
                                                 1, /* only 1 substate */
        CHECK_NULL);
        names_h = objArrayHandle(THREAD, r);
        Handle name = java_lang_String::create_from_str("RUNNABLE", CHECK_NULL);
        names_h->obj_at_put(0, name());
        break;
      }
      case JAVA_THREAD_STATE_BLOCKED : {
        assert(values_h->length() == 1 &&
               values_h->int_at(0) == java_lang_Thread::BLOCKED_ON_MONITOR_ENTER,
               "Invalid threadStatus value");

        objArrayOop r = oopFactory::new_objArray(SystemDictionary::String_klass(),
                                                 1, /* only 1 substate */
        CHECK_NULL);
        names_h = objArrayHandle(THREAD, r);
        Handle name = java_lang_String::create_from_str("BLOCKED", CHECK_NULL);
        names_h->obj_at_put(0, name());
        break;
      }
      case JAVA_THREAD_STATE_WAITING : {
        assert(values_h->length() == 2 &&
               values_h->int_at(0) == java_lang_Thread::IN_OBJECT_WAIT &&
               values_h->int_at(1) == java_lang_Thread::PARKED,
               "Invalid threadStatus value");
        objArrayOop r = oopFactory::new_objArray(SystemDictionary::String_klass(),
                                                 2, /* number of substates */
        CHECK_NULL);
        names_h = objArrayHandle(THREAD, r);
        Handle name0 = java_lang_String::create_from_str("WAITING.OBJECT_WAIT",
        CHECK_NULL);
        Handle name1 = java_lang_String::create_from_str("WAITING.PARKED",
        CHECK_NULL);
        names_h->obj_at_put(0, name0());
        names_h->obj_at_put(1, name1());
        break;
      }
      case JAVA_THREAD_STATE_TIMED_WAITING : {
        assert(values_h->length() == 3 &&
               values_h->int_at(0) == java_lang_Thread::SLEEPING &&
               values_h->int_at(1) == java_lang_Thread::IN_OBJECT_WAIT_TIMED &&
               values_h->int_at(2) == java_lang_Thread::PARKED_TIMED,
               "Invalid threadStatus value");
        objArrayOop r = oopFactory::new_objArray(SystemDictionary::String_klass(),
                                                 3, /* number of substates */
        CHECK_NULL);
        names_h = objArrayHandle(THREAD, r);
        Handle name0 = java_lang_String::create_from_str("TIMED_WAITING.SLEEPING",
        CHECK_NULL);
        Handle name1 = java_lang_String::create_from_str("TIMED_WAITING.OBJECT_WAIT",
        CHECK_NULL);
        Handle name2 = java_lang_String::create_from_str("TIMED_WAITING.PARKED",
        CHECK_NULL);
        names_h->obj_at_put(0, name0());
        names_h->obj_at_put(1, name1());
        names_h->obj_at_put(2, name2());
        break;
      }
      case JAVA_THREAD_STATE_TERMINATED : {
        assert(values_h->length() == 1 &&
               values_h->int_at(0) == java_lang_Thread::TERMINATED,
               "Invalid threadStatus value");
        objArrayOop r = oopFactory::new_objArray(SystemDictionary::String_klass(),
                                                 1, /* only 1 substate */
        CHECK_NULL);
        names_h = objArrayHandle(THREAD, r);
        Handle name = java_lang_String::create_from_str("TERMINATED", CHECK_NULL);
        names_h->obj_at_put(0, name());
        break;
      }
      default:
        // Unknown state - probably incompatible JDK version
        return NULL;
    }
    return (jobjectArray) JNIHandles::make_local(env, names_h());
  }
JVM_END

JVM_ENTRY(void, JVM_GetVersionInfo(JNIEnv* env, jvm_version_info* info, size_t info_size))
  {
    memset(info, 0, info_size);

    info->jvm_version = Abstract_VM_Version::jvm_version();
    info->update_version = 0;          /* 0 in HotSpot Express VM */
    info->special_update_version = 0;  /* 0 in HotSpot Express VM */

    // when we add a new capability in the jvm_version_info struct, we should also
    // consider to expose this new capability in the sun.rt.jvmCapabilities jvmstat
    // counter defined in runtimeService.cpp.
    info->is_attachable = false;
  }
JVM_END