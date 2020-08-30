/*
 * Copyright (c) 1998, 2014, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_CLASSFILE_VERIFIER_HPP
#define SHARE_VM_CLASSFILE_VERIFIER_HPP

#include "classfile/verificationType.hpp"
#include "oops/klass.hpp"
#include "oops/method.hpp"
#include "runtime/handles.hpp"
#include "utilities/growableArray.hpp"
#include "utilities/exceptions.hpp"

// The verifier class
class Verifier : AllStatic {
 public:
  enum {
    STRICTER_ACCESS_CTRL_CHECK_VERSION  = 49,
    STACKMAP_ATTRIBUTE_MAJOR_VERSION    = 50,
    INVOKEDYNAMIC_MAJOR_VERSION         = 51,
    NO_RELAX_ACCESS_CTRL_CHECK_VERSION  = 52
  };
  typedef enum { ThrowException, NoException } Mode;

  /**
   * Verify the bytecodes for a class.  If 'throw_exception' is true
   * then the appropriate VerifyError or ClassFormatError will be thrown.
   * Otherwise, no exception is thrown and the return indicates the
   * error.
   */
  static bool verify(instanceKlassHandle klass, Mode mode, bool should_verify_class, TRAPS);

  // Return false if the class is loaded by the bootstrap loader,
  // or if defineClass was called requesting skipping verification
  // -Xverify:all/none override this value
  static bool should_verify_for(oop class_loader, bool should_verify_class);

  // Relax certain verifier checks to enable some broken 1.1 apps to run on 1.2.
  static bool relax_verify_for(oop class_loader);

 private:
  static bool is_eligible_for_verification(instanceKlassHandle klass, bool should_verify_class);
  static Symbol* inference_verify(
    instanceKlassHandle klass, char* msg, size_t msg_len, TRAPS);
};

class RawBytecodeStream;
class StackMapFrame;
class StackMapTable;

// Summary of verifier's memory usage:
// StackMapTable is stack allocated.
// StackMapFrame are resource allocated. There is only one ResourceMark
// for each class verification, which is created at the top level.
// There is one mutable StackMapFrame (current_frame) which is updated
// by abstract bytecode interpretation. frame_in_exception_handler() returns
// a frame that has a mutable one-item stack (ready for pushing the
// catch type exception object). All the other StackMapFrame's
// are immutable (including their locals and stack arrays) after
// their constructions.
// locals/stack arrays in StackMapFrame are resource allocated.
// locals/stack arrays can be shared between StackMapFrame's, except
// the mutable StackMapFrame (current_frame).

// These macros are used similarly to CHECK macros but also check
// the status of the verifier and return if that has an error.
#define CHECK_VERIFY(verifier) \
  CHECK); if ((verifier)->has_error()) return; ((void)0
#define CHECK_VERIFY_(verifier, result) \
  CHECK_(result)); if ((verifier)->has_error()) return (result); ((void)0

#endif // SHARE_VM_CLASSFILE_VERIFIER_HPP
