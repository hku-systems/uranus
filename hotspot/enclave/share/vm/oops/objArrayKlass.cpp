/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/vmSymbols.hpp"
#include "memory/resourceArea.hpp"
#include "memory/universe.inline.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/klass.inline.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/objArrayKlass.inline.hpp"
#include "oops/objArrayOop.hpp"
#include "oops/oop.inline.hpp"
#include "oops/oop.inline2.hpp"
#include "oops/symbol.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "utilities/copy.hpp"
#include "utilities/macros.hpp"

ObjArrayKlass* ObjArrayKlass::allocate(ClassLoaderData* loader_data, int n, KlassHandle klass_handle, Symbol* name, TRAPS) {
  assert(ObjArrayKlass::header_size() <= InstanceKlass::header_size(),
      "array klasses must be same size as InstanceKlass");

  int size = ArrayKlass::static_size(ObjArrayKlass::header_size());

  return new (loader_data, size, THREAD) ObjArrayKlass(n, klass_handle, name);
}

Klass* ObjArrayKlass::allocate_objArray_klass(ClassLoaderData* loader_data,
                                                int n, KlassHandle element_klass, TRAPS) {
  D_WARN_Unimplement;
}

ObjArrayKlass::ObjArrayKlass(int n, KlassHandle element_klass, Symbol* name) : ArrayKlass(name) {
  this->set_dimension(n);
  this->set_element_klass(element_klass());
  // decrement refcount because object arrays are not explicitly freed.  The
  // InstanceKlass array_name() keeps the name counted while the klass is
  // loaded.
  name->decrement_refcount();

  Klass* bk;
  if (element_klass->oop_is_objArray()) {
    bk = ObjArrayKlass::cast(element_klass())->bottom_klass();
  } else {
    bk = element_klass();
  }
  assert(bk != NULL && (bk->oop_is_instance() || bk->oop_is_typeArray()), "invalid bottom klass");
  this->set_bottom_klass(bk);
  this->set_class_loader_data(bk->class_loader_data());

  this->set_layout_helper(array_layout_helper(T_OBJECT));
  assert(this->oop_is_array(), "sanity");
  assert(this->oop_is_objArray(), "sanity");
}

int ObjArrayKlass::oop_size(oop obj) const {
  assert(obj->is_objArray(), "must be object array");
  return objArrayOop(obj)->object_size();
}

objArrayOop ObjArrayKlass::allocate(int length, TRAPS) {
  D_WARN_Unimplement;
}

static int multi_alloc_counter = 0;

oop ObjArrayKlass::multi_allocate(int rank, jint* sizes, TRAPS) {
  D_WARN_Unimplement;
}

// Either oop or narrowOop depending on UseCompressedOops.
template <class T> void ObjArrayKlass::do_copy(arrayOop s, T* src,
                               arrayOop d, T* dst, int length, TRAPS) {
  D_WARN_Unimplement;
}

void ObjArrayKlass::copy_array(arrayOop s, int src_pos, arrayOop d,
                               int dst_pos, int length, TRAPS) {
  D_WARN_Unimplement;
}


Klass* ObjArrayKlass::array_klass_impl(bool or_null, int n, TRAPS) {

  D_WARN_Unimplement;
}

Klass* ObjArrayKlass::array_klass_impl(bool or_null, TRAPS) {
  return array_klass_impl(or_null, dimension() +  1, CHECK_NULL);
}

bool ObjArrayKlass::can_be_primary_super_slow() const {
  if (!bottom_klass()->can_be_primary_super())
    // array of interfaces
    return false;
  else
    return Klass::can_be_primary_super_slow();
}

GrowableArray<Klass*>* ObjArrayKlass::compute_secondary_supers(int num_extra_slots) {
  // interfaces = { cloneable_klass, serializable_klass, elemSuper[], ... };
  Array<Klass*>* elem_supers = element_klass()->secondary_supers();
  int num_elem_supers = elem_supers == NULL ? 0 : elem_supers->length();
  int num_secondaries = num_extra_slots + 2 + num_elem_supers;
  if (num_secondaries == 2) {
    // Must share this for correct bootstrapping!
    set_secondary_supers(Universe::the_array_interfaces_array());
    return NULL;
  } else {
    GrowableArray<Klass*>* secondaries = new GrowableArray<Klass*>(num_elem_supers+2);
    secondaries->push(SystemDictionary::Cloneable_klass());
    secondaries->push(SystemDictionary::Serializable_klass());
    for (int i = 0; i < num_elem_supers; i++) {
      Klass* elem_super = (Klass*) elem_supers->at(i);
      Klass* array_super = elem_super->array_klass_or_null();
      assert(array_super != NULL, "must already have been created");
      secondaries->push(array_super);
    }
    return secondaries;
  }
}

bool ObjArrayKlass::compute_is_subtype_of(Klass* k) {
  if (!k->oop_is_objArray())
    return ArrayKlass::compute_is_subtype_of(k);

  ObjArrayKlass* oak = ObjArrayKlass::cast(k);
  return element_klass()->is_subtype_of(oak->element_klass());
}

void ObjArrayKlass::initialize(TRAPS) {
  bottom_klass()->initialize(THREAD);  // dispatches to either InstanceKlass or TypeArrayKlass
}

#define ObjArrayKlass_SPECIALIZED_OOP_ITERATE(T, a, p, do_oop) \
{                                   \
  T* p         = (T*)(a)->base();   \
  T* const end = p + (a)->length(); \
  while (p < end) {                 \
    do_oop;                         \
    p++;                            \
  }                                 \
}

#define ObjArrayKlass_SPECIALIZED_BOUNDED_OOP_ITERATE(T, a, p, low, high, do_oop) \
{                                   \
  T* const l = (T*)(low);           \
  T* const h = (T*)(high);          \
  T* p       = (T*)(a)->base();     \
  T* end     = p + (a)->length();   \
  if (p < l) p = l;                 \
  if (end > h) end = h;             \
  while (p < end) {                 \
    do_oop;                         \
    ++p;                            \
  }                                 \
}

#define ObjArrayKlass_OOP_ITERATE(a, p, do_oop)      \
  if (UseCompressedOops) {                           \
    ObjArrayKlass_SPECIALIZED_OOP_ITERATE(narrowOop, \
      a, p, do_oop)                                  \
  } else {                                           \
    ObjArrayKlass_SPECIALIZED_OOP_ITERATE(oop,       \
      a, p, do_oop)                                  \
  }

#define ObjArrayKlass_BOUNDED_OOP_ITERATE(a, p, low, high, do_oop) \
  if (UseCompressedOops) {                                   \
    ObjArrayKlass_SPECIALIZED_BOUNDED_OOP_ITERATE(narrowOop, \
      a, p, low, high, do_oop)                               \
  } else {                                                   \
    ObjArrayKlass_SPECIALIZED_BOUNDED_OOP_ITERATE(oop,       \
      a, p, low, high, do_oop)                               \
  }

void ObjArrayKlass::oop_follow_contents(oop obj) {
  D_WARN_Unimplement;
}

#if INCLUDE_ALL_GCS
void ObjArrayKlass::oop_follow_contents(ParCompactionManager* cm,
                                        oop obj) {
  D_WARN_Unimplement;
}
#endif // INCLUDE_ALL_GCS

#define ObjArrayKlass_OOP_OOP_ITERATE_DEFN(OopClosureType, nv_suffix)           \
                                                                                \
int ObjArrayKlass::oop_oop_iterate##nv_suffix(oop obj,                          \
                                              OopClosureType* closure) {        \
  SpecializationStats::record_iterate_call##nv_suffix(SpecializationStats::oa); \
  assert (obj->is_array(), "obj must be array");                                \
  objArrayOop a = objArrayOop(obj);                                             \
  /* Get size before changing pointers. */                                      \
  /* Don't call size() or oop_size() since that is a virtual call. */           \
  int size = a->object_size();                                                  \
  if_do_metadata_checked(closure, nv_suffix) {                                  \
    closure->do_klass##nv_suffix(obj->klass());                                 \
  }                                                                             \
  ObjArrayKlass_OOP_ITERATE(a, p, (closure)->do_oop##nv_suffix(p))              \
  return size;                                                                  \
}

#define ObjArrayKlass_OOP_OOP_ITERATE_DEFN_m(OopClosureType, nv_suffix)         \
                                                                                \
int ObjArrayKlass::oop_oop_iterate##nv_suffix##_m(oop obj,                      \
                                                  OopClosureType* closure,      \
                                                  MemRegion mr) {               \
  SpecializationStats::record_iterate_call##nv_suffix(SpecializationStats::oa); \
  assert(obj->is_array(), "obj must be array");                                 \
  objArrayOop a  = objArrayOop(obj);                                            \
  /* Get size before changing pointers. */                                      \
  /* Don't call size() or oop_size() since that is a virtual call */            \
  int size = a->object_size();                                                  \
  if_do_metadata_checked(closure, nv_suffix) {                                  \
    /* SSS: Do we need to pass down mr here? */                                 \
    closure->do_klass##nv_suffix(a->klass());                                   \
  }                                                                             \
  ObjArrayKlass_BOUNDED_OOP_ITERATE(                                            \
    a, p, mr.start(), mr.end(), (closure)->do_oop##nv_suffix(p))                \
  return size;                                                                  \
}

// Like oop_oop_iterate but only iterates over a specified range and only used
// for objArrayOops.
#define ObjArrayKlass_OOP_OOP_ITERATE_DEFN_r(OopClosureType, nv_suffix)         \
                                                                                \
int ObjArrayKlass::oop_oop_iterate_range##nv_suffix(oop obj,                    \
                                                  OopClosureType* closure,      \
                                                  int start, int end) {         \
  SpecializationStats::record_iterate_call##nv_suffix(SpecializationStats::oa); \
  assert(obj->is_array(), "obj must be array");                                 \
  objArrayOop a  = objArrayOop(obj);                                            \
  /* Get size before changing pointers. */                                      \
  /* Don't call size() or oop_size() since that is a virtual call */            \
  int size = a->object_size();                                                  \
  if (UseCompressedOops) {                                                      \
    HeapWord* low = start == 0 ? (HeapWord*)a : (HeapWord*)a->obj_at_addr<narrowOop>(start);\
    /* this might be wierd if end needs to be aligned on HeapWord boundary */   \
    HeapWord* high = (HeapWord*)((narrowOop*)a->base() + end);                  \
    MemRegion mr(low, high);                                                    \
    if_do_metadata_checked(closure, nv_suffix) {                                \
      /* SSS: Do we need to pass down mr here? */                               \
      closure->do_klass##nv_suffix(a->klass());                                 \
    }                                                                           \
    ObjArrayKlass_SPECIALIZED_BOUNDED_OOP_ITERATE(narrowOop,                    \
      a, p, low, high, (closure)->do_oop##nv_suffix(p))                         \
  } else {                                                                      \
    HeapWord* low = start == 0 ? (HeapWord*)a : (HeapWord*)a->obj_at_addr<oop>(start);  \
    HeapWord* high = (HeapWord*)((oop*)a->base() + end);                        \
    MemRegion mr(low, high);                                                    \
    if_do_metadata_checked(closure, nv_suffix) {                                \
      /* SSS: Do we need to pass down mr here? */                               \
      closure->do_klass##nv_suffix(a->klass());                                 \
    }                                                                           \
    ObjArrayKlass_SPECIALIZED_BOUNDED_OOP_ITERATE(oop,                          \
      a, p, low, high, (closure)->do_oop##nv_suffix(p))                         \
  }                                                                             \
  return size;                                                                  \
}

int ObjArrayKlass::oop_adjust_pointers(oop obj) {
  D_WARN_Unimplement;
}

#if INCLUDE_ALL_GCS
void ObjArrayKlass::oop_push_contents(PSPromotionManager* pm, oop obj) {
  D_WARN_Unimplement;
}

void ObjArrayKlass::oop_push_contents(ObjArrayKlass* klass, oop obj) {
    ObjArrayKlass_OOP_ITERATE( \
    objArrayOop(obj), p, \
    if (EnclaveGC::is_obj_in_young((address)(*p))) { \
      EnclaveGC::claim_or_forward_depth(p); \
    })
}

void ObjArrayKlass::oop_adjust_pointers(ObjArrayKlass* klass, oop obj) {
  ObjArrayKlass_OOP_ITERATE( \
    objArrayOop(obj), p, \
    if (EnclaveGC::is_obj_in_young((address)(*p))) { \
      EnclaveGC::adjust_pointers(p); \
    })
}

void ObjArrayKlass::oop_copy_contents(ObjArrayKlass* klass, oop obj, std::queue<StarTask> *tasks) {
    assert(obj->is_objArray(), "obj must be obj array");
    ObjArrayKlass_OOP_ITERATE( \
    objArrayOop(obj), p, \
    if (NULL != (*p)) { \
      EnclaveMemory::claim_or_forward_depth(p, tasks); \
    })
}


int ObjArrayKlass::oop_update_pointers(ParCompactionManager* cm, oop obj) {
  D_WARN_Unimplement;
}
#endif // INCLUDE_ALL_GCS

// JVM support

jint ObjArrayKlass::compute_modifier_flags(TRAPS) const {
  // The modifier for an objectArray is the same as its element
  if (element_klass() == NULL) {
    assert(Universe::is_bootstrapping(), "partial objArray only at startup");
    return JVM_ACC_ABSTRACT | JVM_ACC_FINAL | JVM_ACC_PUBLIC;
  }
  // Return the flags of the bottom element type.
  jint element_flags = bottom_klass()->compute_modifier_flags(CHECK_0);

  return (element_flags & (JVM_ACC_PUBLIC | JVM_ACC_PRIVATE | JVM_ACC_PROTECTED))
                        | (JVM_ACC_ABSTRACT | JVM_ACC_FINAL);
}


// Printing

void ObjArrayKlass::print_on(outputStream* st) const {
#ifndef PRODUCT
  Klass::print_on(st);
  st->print(" - instance klass: ");
  element_klass()->print_value_on(st);
  st->cr();
#endif //PRODUCT
}

void ObjArrayKlass::print_value_on(outputStream* st) const {
  assert(is_klass(), "must be klass");

  element_klass()->print_value_on(st);
  st->print("[]");
}

#ifndef PRODUCT

void ObjArrayKlass::oop_print_on(oop obj, outputStream* st) {
  ArrayKlass::oop_print_on(obj, st);
  assert(obj->is_objArray(), "must be objArray");
  objArrayOop oa = objArrayOop(obj);
  int print_len = MIN2((intx) oa->length(), MaxElementPrintSize);
  for(int index = 0; index < print_len; index++) {
    st->print(" - %3d : ", index);
    oa->obj_at(index)->print_value_on(st);
    st->cr();
  }
  int remaining = oa->length() - print_len;
  if (remaining > 0) {
    st->print_cr(" - <%d more elements, increase MaxElementPrintSize to print>", remaining);
  }
}

#endif //PRODUCT

static int max_objArray_print_length = 4;

void ObjArrayKlass::oop_print_value_on(oop obj, outputStream* st) {
  assert(obj->is_objArray(), "must be objArray");
  st->print("a ");
  element_klass()->print_value_on(st);
  int len = objArrayOop(obj)->length();
  st->print("[%d] ", len);
  obj->print_address_on(st);
  if (NOT_PRODUCT(PrintOopAddress ||) PrintMiscellaneous && (WizardMode || Verbose)) {
    st->print("{");
    for (int i = 0; i < len; i++) {
      if (i > max_objArray_print_length) {
        st->print("..."); break;
      }
      st->print(" "INTPTR_FORMAT, (intptr_t)(void*)objArrayOop(obj)->obj_at(i));
    }
    st->print(" }");
  }
}

const char* ObjArrayKlass::internal_name() const {
  return external_name();
}


// Verification

void ObjArrayKlass::verify_on(outputStream* st) {
  ArrayKlass::verify_on(st);
  guarantee(element_klass()->is_klass(), "should be klass");
  guarantee(bottom_klass()->is_klass(), "should be klass");
  Klass* bk = bottom_klass();
  guarantee(bk->oop_is_instance() || bk->oop_is_typeArray(),  "invalid bottom klass");
}

void ObjArrayKlass::oop_verify_on(oop obj, outputStream* st) {
  ArrayKlass::oop_verify_on(obj, st);
  guarantee(obj->is_objArray(), "must be objArray");
  objArrayOop oa = objArrayOop(obj);
  for(int index = 0; index < oa->length(); index++) {
    guarantee(oa->obj_at(index)->is_oop_or_null(), "should be oop");
  }
}
