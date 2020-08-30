/*
 * Copyright (c) 1998, 2017, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/classFileStream.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/verifier.hpp"
#include "classfile/vmSymbols.hpp"
#include "interpreter/bytecodes.hpp"
#include "memory/oopFactory.hpp"
#include "memory/resourceArea.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/oop.inline.hpp"
#include "oops/typeArrayOop.hpp"
#include "prims/jvm.h"
#include "runtime/fieldDescriptor.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "runtime/os.hpp"
#ifdef TARGET_ARCH_x86
# include "bytes_x86.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "bytes_sparc.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "bytes_zero.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "bytes_arm.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "bytes_ppc.hpp"
#endif

#define NOFAILOVER_MAJOR_VERSION                       51
#define NONZERO_PADDING_BYTES_IN_SWITCH_MAJOR_VERSION  51
#define STATIC_METHOD_IN_INTERFACE_MAJOR_VERSION       52

// Access to external entry for VerifyClassCodes - old byte code verifier

extern "C" {
  typedef jboolean (*verify_byte_codes_fn_t)(JNIEnv *, jclass, char *, jint);
  typedef jboolean (*verify_byte_codes_fn_new_t)(JNIEnv *, jclass, char *, jint, jint);
}

static void* volatile _verify_byte_codes_fn = NULL;

static volatile jint _is_new_verify_byte_codes_fn = (jint) true;

static void* verify_byte_codes_fn() {
  if (_verify_byte_codes_fn == NULL) {
    void *lib_handle = os::native_java_library();
    void *func = os::dll_lookup(lib_handle, "VerifyClassCodesForMajorVersion");
    OrderAccess::release_store_ptr(&_verify_byte_codes_fn, func);
    if (func == NULL) {
      OrderAccess::release_store(&_is_new_verify_byte_codes_fn, false);
      func = os::dll_lookup(lib_handle, "VerifyClassCodes");
      OrderAccess::release_store_ptr(&_verify_byte_codes_fn, func);
    }
  }
  return (void*)_verify_byte_codes_fn;
}


// Methods in Verifier

bool Verifier::should_verify_for(oop class_loader, bool should_verify_class) {
  return (class_loader == NULL || !should_verify_class) ?
    BytecodeVerificationLocal : BytecodeVerificationRemote;
}

bool Verifier::relax_verify_for(oop loader) {
  bool trusted = java_lang_ClassLoader::is_trusted_loader(loader);
  bool need_verify =
    // verifyAll
    (BytecodeVerificationLocal && BytecodeVerificationRemote) ||
    // verifyRemote
    (!BytecodeVerificationLocal && BytecodeVerificationRemote && !trusted);
  return !need_verify;
}

bool Verifier::verify(instanceKlassHandle klass, Verifier::Mode mode, bool should_verify_class, TRAPS) {
    D_WARN_Unimplement;
}

bool Verifier::is_eligible_for_verification(instanceKlassHandle klass, bool should_verify_class) {
    return false;
}

Symbol* Verifier::inference_verify(
    instanceKlassHandle klass, char* message, size_t message_len, TRAPS) {
    D_WARN_Unimplement;
    return NULL;
}