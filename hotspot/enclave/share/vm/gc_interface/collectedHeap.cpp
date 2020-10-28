/*
 * Copyright (c) 2001, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include "gc_interface/collectedHeap.hpp"
#include "gc_interface/collectedHeap.inline.hpp"
#include "oops/oop.inline.hpp"
#include "oops/instanceMirrorKlass.hpp"
#include "runtime/thread.inline.hpp"


#ifdef ASSERT
int CollectedHeap::_fire_out_of_memory_count = 0;
#endif


void GCHeapLog::log_heap(bool before) {

}

CollectedHeap::CollectedHeap()
{
  // Create the ring log
  if (LogEvents) {
    _gc_heap_log = new GCHeapLog();
  } else {
    _gc_heap_log = NULL;
  }
}

#include "gc_interface/collectedHeap.hpp"
#include "memory/universe.hpp"
#include "oops/arrayOop.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/copy.hpp"

// Inline allocation implementations.

void CollectedHeap::post_allocation_setup_common(KlassHandle klass,
                                                 HeapWord* obj) {
  post_allocation_setup_no_klass_install(klass, obj);
  post_allocation_install_obj_klass(klass, oop(obj));
}

void CollectedHeap::post_allocation_setup_no_klass_install(KlassHandle klass,
                                                           HeapWord* objPtr) {
  oop obj = (oop)objPtr;

  assert(obj != NULL, "NULL object pointer");
  if (UseBiasedLocking && (klass() != NULL)) {
    obj->set_mark(klass->prototype_header());
  } else {
    // May be bootstrapping
    obj->set_mark(markOopDesc::prototype());
  }
}

void CollectedHeap::post_allocation_install_obj_klass(KlassHandle klass,
                                                      oop obj) {
  // These asserts are kind of complicated because of klassKlass
  // and the beginning of the world.
  assert(klass() != NULL || !Universe::is_fully_initialized(), "NULL klass");
  assert(klass() == NULL || klass()->is_klass(), "not a klass");
  assert(obj != NULL, "NULL object pointer");
  obj->set_klass(klass());
  assert(!Universe::is_fully_initialized() || obj->klass() != NULL,
         "missing klass");
}

// Support for jvmti and dtrace
inline void post_allocation_notify(KlassHandle klass, oop obj, int size) {

}

void CollectedHeap::post_allocation_setup_obj(KlassHandle klass,
                                              HeapWord* obj,
                                              int size) {
  post_allocation_setup_common(klass, obj);
  assert(Universe::is_bootstrapping() ||
         !((oop)obj)->is_array(), "must not be an array");
  // notify jvmti and dtrace
  post_allocation_notify(klass, (oop)obj, size);
}

void CollectedHeap::post_allocation_setup_array(KlassHandle klass,
                                                HeapWord* obj,
                                                int length) {
  // Set array length before setting the _klass field
  // in post_allocation_setup_common() because the klass field
  // indicates that the object is parsable by concurrent GC.
  assert(length >= 0, "length should be non-negative");
  ((arrayOop)obj)->set_length(length);
  post_allocation_setup_common(klass, obj);
  oop new_obj = (oop)obj;
  assert(new_obj->is_array(), "must be an array");
  // notify jvmti and dtrace (must be after length is set for dtrace)
  post_allocation_notify(klass, new_obj, new_obj->size());
}

HeapWord* CollectedHeap::common_mem_allocate_noinit(KlassHandle klass, size_t size, TRAPS) {

  // Clear unhandled oops for memory allocation.  Memory allocation might
  // not take out a lock if from tlab, so clear here.
  CHECK_UNHANDLED_OOPS_ONLY(THREAD->clear_unhandled_oops();)

  if (HAS_PENDING_EXCEPTION) {
    NOT_PRODUCT(guarantee(false, "Should not allocate with exception pending"));
    return NULL;  // caller does a CHECK_0 too
  }

  HeapWord* result = NULL;
  if (UseTLAB) {
    result = allocate_from_tlab(klass, THREAD, size);
    if (result != NULL) {
      assert(!HAS_PENDING_EXCEPTION,
             "Unexpected exception, will result in uninitialized storage");
      return result;
    }
  }
  bool gc_overhead_limit_was_exceeded = false;
  result = Universe::heap()->mem_allocate(size,
                                          &gc_overhead_limit_was_exceeded);
  if (result != NULL) {
    NOT_PRODUCT(Universe::heap()->
            check_for_non_bad_heap_word_value(result, size));
    assert(!HAS_PENDING_EXCEPTION,
           "Unexpected exception, will result in uninitialized storage");

    return result;
  }


  if (!gc_overhead_limit_was_exceeded) {
    // -XX:+HeapDumpOnOutOfMemoryError and -XX:OnOutOfMemoryError support

    THROW_OOP_0(Universe::out_of_memory_error_java_heap());
  } else {

    THROW_OOP_0(Universe::out_of_memory_error_gc_overhead_limit());
  }
}

HeapWord* CollectedHeap::common_mem_allocate_init(KlassHandle klass, size_t size, TRAPS) {
  HeapWord* obj = common_mem_allocate_noinit(klass, size, CHECK_NULL);
  init_obj(obj, size);
  return obj;
}

HeapWord* CollectedHeap::allocate_from_tlab(KlassHandle klass, Thread* thread, size_t size) {
  assert(UseTLAB, "should use UseTLAB");

  HeapWord* obj = (HeapWord*)EnclaveMemory::enclaveMemory->allocate(size, (JavaThread*)thread);
  return obj;
}

void CollectedHeap::init_obj(HeapWord* obj, size_t size) {
  assert(obj != NULL, "cannot initialize NULL object");
  const size_t hs = oopDesc::header_size();
  assert(size >= hs, "unexpected object size");
  ((oop)obj)->set_klass_gap(0);
  Copy::fill_to_aligned_words(obj + hs, size - hs);
}

oop CollectedHeap::obj_allocate(KlassHandle klass, int size, TRAPS) {
  debug_only(check_for_valid_allocation_state());
  assert(!Universe::heap()->is_gc_active(), "Allocation during gc not allowed");
  assert(size >= 0, "int won't convert to size_t");
  HeapWord* obj = common_mem_allocate_init(klass, size, CHECK_NULL);
  post_allocation_setup_obj(klass, obj, size);
  NOT_PRODUCT(Universe::heap()->check_for_bad_heap_word_value(obj, size));
  return (oop)obj;
}

oop CollectedHeap::array_allocate(KlassHandle klass,
                                  int size,
                                  int length,
                                  TRAPS) {
  debug_only(check_for_valid_allocation_state());
  assert(!Universe::heap()->is_gc_active(), "Allocation during gc not allowed");
  assert(size >= 0, "int won't convert to size_t");
  HeapWord* obj = common_mem_allocate_init(klass, size, CHECK_NULL);
  post_allocation_setup_array(klass, obj, length);
  NOT_PRODUCT(Universe::heap()->check_for_bad_heap_word_value(obj, size));
  return (oop)obj;
}

oop CollectedHeap::array_allocate_nozero(KlassHandle klass,
                                         int size,
                                         int length,
                                         TRAPS) {
  debug_only(check_for_valid_allocation_state());
  assert(!Universe::heap()->is_gc_active(), "Allocation during gc not allowed");
  assert(size >= 0, "int won't convert to size_t");
  HeapWord* obj = common_mem_allocate_noinit(klass, size, CHECK_NULL);
  ((oop)obj)->set_klass_gap(0);
  post_allocation_setup_array(klass, obj, length);
#ifndef PRODUCT
  const size_t hs = oopDesc::header_size()+1;
  Universe::heap()->check_for_non_bad_heap_word_value(obj+hs, size-hs);
#endif
  return (oop)obj;
}

HeapWord* CollectedHeap::mem_allocate(size_t size,
                       bool* gc_overhead_limit_was_exceeded) {
  return (HeapWord*)EnclaveMemory::enclaveMemory->alloc(size, JavaThread::current());
}
