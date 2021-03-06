/*
 * Copyright (c) 2019, Red Hat, Inc. All rights reserved.
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
#ifndef SHARE_GC_SHENANDOAH_SHENANDOAHCLOSURES_INLINE_HPP
#define SHARE_GC_SHENANDOAH_SHENANDOAHCLOSURES_INLINE_HPP

#include "gc_implementation/shenandoah/shenandoahAsserts.hpp"
#include "gc_implementation/shenandoah/shenandoahClosures.hpp"
#include "gc_implementation/shenandoah/shenandoahHeap.inline.hpp"
#include "runtime/thread.hpp"

ShenandoahForwardedIsAliveClosure::ShenandoahForwardedIsAliveClosure() :
  _mark_context(ShenandoahHeap::heap()->marking_context()) {
}

bool ShenandoahForwardedIsAliveClosure::do_object_b(oop obj) {
  if (oopDesc::is_null(obj)) {
    return false;
  }
  obj = ShenandoahBarrierSet::resolve_forwarded_not_null(obj);
  shenandoah_assert_not_forwarded_if(NULL, obj,
                                     ShenandoahHeap::heap()->is_concurrent_mark_in_progress());
  return _mark_context->is_marked(obj);
}

ShenandoahIsAliveClosure::ShenandoahIsAliveClosure() :
  _mark_context(ShenandoahHeap::heap()->marking_context()) {
}

bool ShenandoahIsAliveClosure::do_object_b(oop obj) {
  if (oopDesc::is_null(obj)) {
    return false;
  }
  shenandoah_assert_not_forwarded(NULL, obj);
  return _mark_context->is_marked(obj);
}

BoolObjectClosure* ShenandoahIsAliveSelector::is_alive_closure() {
  return ShenandoahHeap::heap()->has_forwarded_objects() ?
         reinterpret_cast<BoolObjectClosure*>(&_fwd_alive_cl) :
         reinterpret_cast<BoolObjectClosure*>(&_alive_cl);
}

ShenandoahUpdateRefsClosure::ShenandoahUpdateRefsClosure() :
  _heap(ShenandoahHeap::heap()) {
}

template <class T>
void ShenandoahUpdateRefsClosure::do_oop_work(T* p) {
  T o = oopDesc::load_heap_oop(p);
  if (!oopDesc::is_null(o)) {
    oop obj = oopDesc::decode_heap_oop_not_null(o);
    _heap->update_with_forwarded_not_null(p, obj);
  }
}

void ShenandoahUpdateRefsClosure::do_oop(oop* p)       { do_oop_work(p); }
void ShenandoahUpdateRefsClosure::do_oop(narrowOop* p) { do_oop_work(p); }

ShenandoahEvacuateUpdateRootsClosure::ShenandoahEvacuateUpdateRootsClosure() :
  _heap(ShenandoahHeap::heap()), _thread(Thread::current()) {
}

template <class T>
void ShenandoahEvacuateUpdateRootsClosure::do_oop_work(T* p) {
  assert(_heap->is_evacuation_in_progress(), "Only do this when evacuation is in progress");

  T o = oopDesc::load_heap_oop(p);
  if (! oopDesc::is_null(o)) {
    oop obj = oopDesc::decode_heap_oop_not_null(o);
    if (_heap->in_collection_set(obj)) {
      shenandoah_assert_marked(p, obj);
      oop resolved = ShenandoahBarrierSet::resolve_forwarded_not_null(obj);
      if (oopDesc::unsafe_equals(resolved, obj)) {
	bool evac;
        resolved = _heap->evacuate_object(obj, _thread, evac);
      }
      oopDesc::encode_store_heap_oop(p, resolved);
    }
  }
}

void ShenandoahEvacuateUpdateRootsClosure::do_oop(oop* p) {
  do_oop_work(p);
}

void ShenandoahEvacuateUpdateRootsClosure::do_oop(narrowOop* p) {
  do_oop_work(p);
}

#endif // SHARE_GC_SHENANDOAH_SHENANDOAHCLOSURES_HPP
