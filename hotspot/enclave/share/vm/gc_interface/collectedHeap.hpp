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

#ifndef SHARE_VM_GC_INTERFACE_COLLECTEDHEAP_HPP
#define SHARE_VM_GC_INTERFACE_COLLECTEDHEAP_HPP


#include "memory/allocation.hpp"
#include "runtime/handles.hpp"

// A "CollectedHeap" is an implementation of a java heap for HotSpot.  This
// is an abstract class: there may be many different kinds of heaps.  This
// class defines the functions that a heap must implement, and contains
// infrastructure common to all heaps.

class AdaptiveSizePolicy;
class BarrierSet;
class CollectorPolicy;
class GCHeapSummary;
class GCTimer;
class GCTracer;
class MetaspaceSummary;
class Thread;
class ThreadClosure;
class VirtualSpaceSummary;
class nmethod;

class GCMessage {
 public:
  bool is_before;

 public:
  GCMessage() {}
};

class GCHeapLog  {
 private:
  void log_heap(bool before);

 public:
  GCHeapLog() {}

  void log_heap_before() {
    log_heap(true);
  }
  void log_heap_after() {
    log_heap(false);
  }
};

//
// CollectedHeap
//   SharedHeap
//     GenCollectedHeap
//     G1CollectedHeap
//   ParallelScavengeHeap
//   ShenandoahHeap
//
class CollectedHeap : public CHeapObj<mtInternal> {
  friend class VMStructs;

  GCHeapLog* _gc_heap_log;


protected:
  // Allocate from the current thread's TLAB, with broken-out slow path.
  static HeapWord* allocate_from_tlab(KlassHandle klass, Thread* thread, size_t size);

  // Allocate an uninitialized block of the given size, or returns NULL if
  // this is impossible.
  static HeapWord* common_mem_allocate_noinit(KlassHandle klass, size_t size, TRAPS);

  // Like allocate_init, but the block returned by a successful allocation
  // is guaranteed initialized to zeros.
  static HeapWord* common_mem_allocate_init(KlassHandle klass, size_t size, TRAPS);

  // Helper functions for (VM) allocation.
  static void post_allocation_setup_common(KlassHandle klass, HeapWord* obj);
  static void post_allocation_setup_no_klass_install(KlassHandle klass,
                                                            HeapWord* objPtr);

  static void post_allocation_setup_obj(KlassHandle klass, HeapWord* obj, int size);

  static void post_allocation_setup_array(KlassHandle klass,
                                                 HeapWord* obj, int length);

  // Clears an allocated object.
  static void init_obj(HeapWord* obj, size_t size);

  DEBUG_ONLY(static void fill_args_check(HeapWord* start, size_t words);)
  DEBUG_ONLY(static void zap_filler_array(HeapWord* start, size_t words, bool zap = true);)

  // Verification functions
  virtual void check_for_bad_heap_word_value(HeapWord* addr, size_t size)
    PRODUCT_RETURN;
  virtual void check_for_non_bad_heap_word_value(HeapWord* addr, size_t size)
    PRODUCT_RETURN;
  debug_only(static void check_for_valid_allocation_state();)

 public:
  // Constructor
  CollectedHeap();
  // General obj/array allocation facilities.
  static oop obj_allocate(KlassHandle klass, int size, TRAPS);
  static oop array_allocate(KlassHandle klass, int size, int length, TRAPS);
  static oop array_allocate_nozero(KlassHandle klass, int size, int length, TRAPS);

  static void post_allocation_install_obj_klass(KlassHandle klass,
                                                       oop obj);


  // Raw memory allocation facilities
  // The obj and array allocate methods are covers for these methods.
  // mem_allocate() should never be
  // called to allocate TLABs, only individual objects.
  HeapWord* mem_allocate(size_t size,
                                 bool* gc_overhead_limit_was_exceeded);

  // These functions return the addresses of the fields that define the
  // boundaries of the contiguous allocation area.  (These fields should be
  // physically near to one another.)
  HeapWord** top_addr() const {
    guarantee(false, "inline contiguous allocation not supported");
    return NULL;
  }
  HeapWord** end_addr() const {
    guarantee(false, "inline contiguous allocation not supported");
    return NULL;
  }

};

#endif // SHARE_VM_GC_INTERFACE_COLLECTEDHEAP_HPP
