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
#include "classfile/classLoader.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "memory/oopFactory.hpp"
#include "memory/universe.hpp"
#include "oops/constantPool.hpp"
#include "oops/generateOopMap.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/instanceOop.hpp"
#include "oops/method.hpp"
#include "oops/objArrayOop.hpp"
#include "oops/oop.inline.hpp"
#include "oops/symbol.hpp"
#include "runtime/java.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/macros.hpp"

#ifdef TARGET_ARCH_x86
# include "vm_version_x86.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "vm_version_sparc.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "vm_version_zero.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "vm_version_arm.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "vm_version_ppc.hpp"
#endif

JDK_Version JDK_Version::_current;
const char* JDK_Version::_runtime_name;
const char* JDK_Version::_runtime_version;

void JDK_Version::initialize() {
  jdk_version_info info;
  assert(!_current.is_valid(), "Don't initialize twice");

  void *lib_handle = os::native_java_library();
  jdk_version_info_fn_t func = CAST_TO_FN_PTR(jdk_version_info_fn_t,
     os::dll_lookup(lib_handle, "JDK_GetVersionInfo0"));

  if (func == NULL) {
    // JDK older than 1.6
    _current._partially_initialized = true;
  } else {
    (*func)(&info, sizeof(info));

    int major = JDK_VERSION_MAJOR(info.jdk_version);
    int minor = JDK_VERSION_MINOR(info.jdk_version);
    int micro = JDK_VERSION_MICRO(info.jdk_version);
    int build = JDK_VERSION_BUILD(info.jdk_version);
    if (major == 1 && minor > 4) {
      // We represent "1.5.0" as "5.0", but 1.4.2 as itself.
      major = minor;
      minor = micro;
      micro = 0;
    }
    _current = JDK_Version(major, minor, micro, info.update_version,
                           info.special_update_version, build,
                           info.thread_park_blocker == 1,
                           info.post_vm_init_hook_enabled == 1,
                           info.pending_list_uses_discovered_field == 1);
  }
}

void JDK_Version::fully_initialize(
    uint8_t major, uint8_t minor, uint8_t micro, uint8_t update) {
  // This is only called when current is less than 1.6 and we've gotten
  // far enough in the initialization to determine the exact version.
  assert(major < 6, "not needed for JDK version >= 6");
  assert(is_partially_initialized(), "must not initialize");
  if (major < 5) {
    // JDK verison sequence: 1.2.x, 1.3.x, 1.4.x, 5.0.x, 6.0.x, etc.
    micro = minor;
    minor = major;
    major = 1;
  }
  _current = JDK_Version(major, minor, micro, update);
}

void JDK_Version_init() {
  JDK_Version::initialize();
}

static int64_t encode_jdk_version(const JDK_Version& v) {
  return
    ((int64_t)v.major_version()          << (BitsPerByte * 5)) |
    ((int64_t)v.minor_version()          << (BitsPerByte * 4)) |
    ((int64_t)v.micro_version()          << (BitsPerByte * 3)) |
    ((int64_t)v.update_version()         << (BitsPerByte * 2)) |
    ((int64_t)v.special_update_version() << (BitsPerByte * 1)) |
    ((int64_t)v.build_number()           << (BitsPerByte * 0));
}

int JDK_Version::compare(const JDK_Version& other) const {
  assert(is_valid() && other.is_valid(), "Invalid version (uninitialized?)");
  if (!is_partially_initialized() && other.is_partially_initialized()) {
    return -(other.compare(*this)); // flip the comparators
  }
  assert(!other.is_partially_initialized(), "Not initialized yet");
  if (is_partially_initialized()) {
    assert(other.major_version() >= 6,
           "Invalid JDK version comparison during initialization");
    return -1;
  } else {
    uint64_t e = encode_jdk_version(*this);
    uint64_t o = encode_jdk_version(other);
    return (e > o) ? 1 : ((e == o) ? 0 : -1);
  }
}

void JDK_Version::to_string(char* buffer, size_t buflen) const {
  assert(buffer && buflen > 0, "call with useful buffer");
  size_t index = 0;
  if (!is_valid()) {
    jio_snprintf(buffer, buflen, "%s", "(uninitialized)");
  } else if (is_partially_initialized()) {
    jio_snprintf(buffer, buflen, "%s", "(uninitialized) pre-1.6.0");
  } else {
    int rc = jio_snprintf(
        &buffer[index], buflen - index, "%d.%d", _major, _minor);
    if (rc == -1) return;
    index += rc;
    if (_micro > 0) {
      rc = jio_snprintf(&buffer[index], buflen - index, ".%d", _micro);
      if (rc == -1) return;
      index += rc;
    }
    if (_update > 0) {
      rc = jio_snprintf(&buffer[index], buflen - index, "_%02d", _update);
      if (rc == -1) return;
      index += rc;
    }
    if (_special > 0) {
      rc = jio_snprintf(&buffer[index], buflen - index, "%c", _special);
      if (rc == -1) return;
      index += rc;
    }
    if (_build > 0) {
      rc = jio_snprintf(&buffer[index], buflen - index, "-b%02d", _build);
      if (rc == -1) return;
      index += rc;
    }
  }
}

void vm_exit_during_initialization(Handle exception) {
  char* null_ptr = NULL;
  char c = *null_ptr;
  return;
}

void vm_exit_during_initialization(Symbol* ex, const char* message) {
  char* null_ptr = NULL;
  printf("%s\n", message);
  char c = *null_ptr;
  return;
}

void vm_exit_during_initialization(const char* error, const char* message) {
  char* null_ptr = NULL;
  printf("%s %s\n", error, message);
  char c = *null_ptr;
  return;
}
