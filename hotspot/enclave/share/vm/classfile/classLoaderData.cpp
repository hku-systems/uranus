/*
 * Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.
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

// A ClassLoaderData identifies the full set of class types that a class
// loader's name resolution strategy produces for a given configuration of the
// class loader.
// Class types in the ClassLoaderData may be defined by from class file binaries
// provided by the class loader, or from other class loader it interacts with
// according to its name resolution strategy.
//
// Class loaders that implement a deterministic name resolution strategy
// (including with respect to their delegation behavior), such as the boot, the
// extension, and the system loaders of the JDK's built-in class loader
// hierarchy, always produce the same linkset for a given configuration.
//
// ClassLoaderData carries information related to a linkset (e.g.,
// metaspace holding its klass definitions).
// The System Dictionary and related data structures (e.g., placeholder table,
// loader constraints table) as well as the runtime representation of classes
// only reference ClassLoaderData.
//
// Instances of java.lang.ClassLoader holds a pointer to a ClassLoaderData that
// that represent the loader's "linking domain" in the JVM.
//
// The bootstrap loader (represented by NULL) also has a ClassLoaderData,
// the singleton class the_null_class_loader_data().

#include "precompiled.hpp"
#include "classfile/classLoaderData.hpp"
#include "classfile/classLoaderData.inline.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/systemDictionary.hpp"
#include "runtime/synchronizer.hpp"
#include "utilities/growableArray.hpp"
#include "utilities/macros.hpp"
#include "utilities/ostream.hpp"

ClassLoaderData * ClassLoaderData::_the_null_class_loader_data = NULL;

ClassLoaderData::ClassLoaderData(Handle h_class_loader, bool is_anonymous, Dependencies dependencies) :
  _class_loader(h_class_loader()),
  _is_anonymous(is_anonymous),
  // An anonymous class loader data doesn't have anything to keep
  // it from being unloaded during parsing of the anonymous class.
  // The null-class-loader should always be kept alive.
  _keep_alive(is_anonymous || h_class_loader.is_null()),
  _unloading(false), _klasses(NULL),
  _claimed(0), _jmethod_ids(NULL), _handles(), _deallocate_list(NULL),
  _next(NULL), _dependencies(dependencies),
  _metaspace_lock(NULL) {
    // empty
}

void ClassLoaderData::init_dependencies(TRAPS) {
  assert(!Universe::is_fully_initialized(), "should only be called when initializing");
  assert(is_the_null_class_loader_data(), "should only call this for the null class loader");
  _dependencies.init(CHECK);
}

void ClassLoaderData::Dependencies::init(TRAPS) {
  // Create empty dependencies array to add to. CMS requires this to be
  // an oop so that it can track additions via card marks.  We think.
}

ClassLoaderData::ChunkedHandleList::~ChunkedHandleList() {
  Chunk* c = _head;
  while (c != NULL) {
    Chunk* next = c->_next;
    delete c;
    c = next;
  }
}

oop* ClassLoaderData::ChunkedHandleList::add(oop o) {
  if (_head == NULL || _head->_size == Chunk::CAPACITY) {
    Chunk* next = new Chunk(_head);
    OrderAccess::release_store_ptr(&_head, next);
  }
  oop* handle = &_head->_data[_head->_size];
  *handle = o;
  OrderAccess::release_store(&_head->_size, _head->_size + 1);
  return handle;
}

inline void ClassLoaderData::ChunkedHandleList::oops_do_chunk(OopClosure* f, Chunk* c, const juint size) {
  for (juint i = 0; i < size; i++) {
    if (c->_data[i] != NULL) {
      f->do_oop(&c->_data[i]);
    }
  }
}

void ClassLoaderData::ChunkedHandleList::oops_do(OopClosure* f) {
  Chunk* head = (Chunk*) OrderAccess::load_ptr_acquire(&_head);
  if (head != NULL) {
    // Must be careful when reading size of head
    oops_do_chunk(f, head, OrderAccess::load_acquire(&head->_size));
    for (Chunk* c = head->_next; c != NULL; c = c->_next) {
      oops_do_chunk(f, c, c->_size);
    }
  }
}

bool ClassLoaderData::claim() {
  if (_claimed == 1) {
    return false;
  }

  return (int) Atomic::cmpxchg(1, &_claimed, 0) == 0;
}

void ClassLoaderData::oops_do(OopClosure* f, KlassClosure* klass_closure, bool must_claim) {
  if (must_claim && !claim()) {
    return;
  }

  f->do_oop(&_class_loader);
  _dependencies.oops_do(f);
  _handles.oops_do(f);
  if (klass_closure != NULL) {
    classes_do(klass_closure);
  }
}

void ClassLoaderData::Dependencies::oops_do(OopClosure* f) {
  f->do_oop((oop*)&_list_head);
}

void ClassLoaderData::classes_do(KlassClosure* klass_closure) {
  for (Klass* k = _klasses; k != NULL; k = k->next_link()) {
    klass_closure->do_klass(k);
    assert(k != k->next_link(), "no loops!");
  }
}

void ClassLoaderData::classes_do(void f(Klass * const)) {
  for (Klass* k = _klasses; k != NULL; k = k->next_link()) {
    f(k);
  }
}

void ClassLoaderData::loaded_classes_do(KlassClosure* klass_closure) {

}

void ClassLoaderData::classes_do(void f(InstanceKlass*)) {
  for (Klass* k = _klasses; k != NULL; k = k->next_link()) {
    if (k->oop_is_instance()) {
      f(InstanceKlass::cast(k));
    }
    assert(k != k->next_link(), "no loops!");
  }
}

void ClassLoaderData::record_dependency(Klass* k, TRAPS) {
  ClassLoaderData * const from_cld = this;
  ClassLoaderData * const to_cld = k->class_loader_data();

  // Dependency to the null class loader data doesn't need to be recorded
  // because the null class loader data never goes away.
  if (to_cld->is_the_null_class_loader_data()) {
    return;
  }

  oop to;
  if (to_cld->is_anonymous()) {
    // Anonymous class dependencies are through the mirror.
    to = k->java_mirror();
  } else {
    to = to_cld->class_loader();

    // If from_cld is anonymous, even if it's class_loader is a parent of 'to'
    // we still have to add it.  The class_loader won't keep from_cld alive.
    if (!from_cld->is_anonymous()) {
      // Check that this dependency isn't from the same or parent class_loader
      oop from = from_cld->class_loader();

      oop curr = from;
      while (curr != NULL) {
        if (curr == to) {
          return; // this class loader is in the parent list, no need to add it.
        }
        curr = java_lang_ClassLoader::parent(curr);
      }
    }
  }

  // It's a dependency we won't find through GC, add it. This is relatively rare
  // Must handle over GC point.
  Handle dependency(THREAD, to);
  from_cld->_dependencies.add(dependency, CHECK);
}


void ClassLoaderData::Dependencies::add(Handle dependency, TRAPS) {
  D_WARN_Unimplement;
}

void ClassLoaderData::Dependencies::locked_add(objArrayHandle last_handle,
                                               objArrayHandle new_dependency,
                                               Thread* THREAD) {
  D_WARN_Unimplement;
}

void ClassLoaderDataGraph::clear_claimed_marks() {
  for (ClassLoaderData* cld = _head; cld != NULL; cld = cld->next()) {
    cld->clear_claimed();
  }
}

void ClassLoaderData::add_class(Klass* k) {
  D_WARN_Unimplement;
}

// This is called by InstanceKlass::deallocate_contents() to remove the
// scratch_class for redefine classes.  We need a lock because there it may not
// be called at a safepoint if there's an error.
void ClassLoaderData::remove_class(Klass* scratch_class) {
  D_WARN_Unimplement;
}

void ClassLoaderData::unload() {
  _unloading = true;

  // Tell serviceability tools these classes are unloading
  classes_do(InstanceKlass::notify_unload_class);

  if (TraceClassLoaderData) {
    ResourceMark rm;
    tty->print("[ClassLoaderData: unload loader data " INTPTR_FORMAT, p2i(this));
    tty->print(" for instance " INTPTR_FORMAT " of %s", p2i((void *)class_loader()),
               loader_name());
    if (is_anonymous()) {
      tty->print(" for anonymous class  " INTPTR_FORMAT " ", p2i(_klasses));
    }
    tty->print_cr("]");
  }
}

oop ClassLoaderData::keep_alive_object() const {
  assert(!keep_alive(), "Don't use with CLDs that are artificially kept alive");
  return is_anonymous() ? _klasses->java_mirror() : class_loader();
}

bool ClassLoaderData::is_alive(BoolObjectClosure* is_alive_closure) const {
  bool alive = keep_alive() // null class loader and incomplete anonymous klasses.
      || is_alive_closure->do_object_b(keep_alive_object());

  return alive;
}


ClassLoaderData::~ClassLoaderData() {
  D_WARN_Unimplement;
}

/**
 * Returns true if this class loader data is for the extension class loader.
 */
bool ClassLoaderData::is_ext_class_loader_data() const {
  return SystemDictionary::is_ext_class_loader(class_loader());
}

jobject ClassLoaderData::add_handle(Handle h) {
  return (jobject) _handles.add(h());
}

// Add this metadata pointer to be freed when it's safe.  This is only during
// class unloading because Handles might point to this metadata field.
void ClassLoaderData::add_to_deallocate_list(Metadata* m) {
  // Metadata in shared region isn't deleted.
  if (!m->is_shared()) {
    if (_deallocate_list == NULL) {
      _deallocate_list = new (ResourceObj::C_HEAP, mtClass) GrowableArray<Metadata*>(100, true);
    }
    _deallocate_list->append_if_missing(m);
  }
}

// Deallocate free metadata on the free list.  How useful the PermGen was!
void ClassLoaderData::free_deallocate_list() {
  D_WARN_Unimplement;
}

// These anonymous class loaders are to contain classes used for JSR292
ClassLoaderData* ClassLoaderData::anonymous_class_loader_data(oop loader, TRAPS) {
  // Add a new class loader data to the graph.
  return ClassLoaderDataGraph::add(loader, true, CHECK_NULL);
}

const char* ClassLoaderData::loader_name() {
  // Handles null class loader
  return SystemDictionary::loader_name(class_loader());
}

#ifndef PRODUCT
// Define to dump klasses
#undef CLD_DUMP_KLASSES

void ClassLoaderData::dump(outputStream * const out) {
  ResourceMark rm;
  out->print("ClassLoaderData CLD: "PTR_FORMAT", loader: "PTR_FORMAT", loader_klass: "PTR_FORMAT" %s {",
      p2i(this), p2i((void *)class_loader()),
      p2i(class_loader() != NULL ? class_loader()->klass() : NULL), loader_name());
  if (claimed()) out->print(" claimed ");
  if (is_unloading()) out->print(" unloading ");
  out->cr();
  if (metaspace_or_null() != NULL) {
    out->print_cr("metaspace: " INTPTR_FORMAT, p2i(metaspace_or_null()));
    metaspace_or_null()->dump(out);
  } else {
    out->print_cr("metaspace: NULL");
  }

#ifdef CLD_DUMP_KLASSES
  if (Verbose) {
    ResourceMark rm;
    Klass* k = _klasses;
    while (k != NULL) {
      out->print_cr("klass "PTR_FORMAT", %s, CT: %d, MUT: %d", k, k->name()->as_C_string(),
          k->has_modified_oops(), k->has_accumulated_modified_oops());
      assert(k != k->next_link(), "no loops!");
      k = k->next_link();
    }
  }
#endif  // CLD_DUMP_KLASSES
#undef CLD_DUMP_KLASSES
  if (_jmethod_ids != NULL) {
    Method::print_jmethod_ids(this, out);
  }
  out->print_cr("}");
}
#endif // PRODUCT

void ClassLoaderData::verify() {
  D_WARN_Unimplement;
}

bool ClassLoaderData::contains_klass(Klass* klass) {
  for (Klass* k = _klasses; k != NULL; k = k->next_link()) {
    if (k == klass) return true;
  }
  return false;
}


// GC root of class loader data created.
ClassLoaderData* ClassLoaderDataGraph::_head = NULL;
ClassLoaderData* ClassLoaderDataGraph::_unloading = NULL;
ClassLoaderData* ClassLoaderDataGraph::_saved_unloading = NULL;
ClassLoaderData* ClassLoaderDataGraph::_saved_head = NULL;

bool ClassLoaderDataGraph::_should_purge = false;

// Add a new class loader data node to the list.  Assign the newly created
// ClassLoaderData into the java/lang/ClassLoader object as a hidden field
ClassLoaderData* ClassLoaderDataGraph::add(Handle loader, bool is_anonymous, TRAPS) {
  // We need to allocate all the oops for the ClassLoaderData before allocating the
  // actual ClassLoaderData object.
  ClassLoaderData::Dependencies dependencies(CHECK_NULL);

  ClassLoaderData* cld = new ClassLoaderData(loader, is_anonymous, dependencies);


  if (!is_anonymous) {
    ClassLoaderData** cld_addr = java_lang_ClassLoader::loader_data_addr(loader());
    // First, Atomically set it
    ClassLoaderData* old = (ClassLoaderData*) Atomic::cmpxchg_ptr(cld, cld_addr, NULL);
    if (old != NULL) {
      delete cld;
      // Returns the data.
      return old;
    }
  }

  // We won the race, and therefore the task of adding the data to the list of
  // class loader data
  ClassLoaderData** list_head = &_head;
  ClassLoaderData* next = _head;

  do {
    cld->set_next(next);
    ClassLoaderData* exchanged = (ClassLoaderData*)Atomic::cmpxchg_ptr(cld, list_head, next);
    if (exchanged == next) {
      if (TraceClassLoaderData) {
        ResourceMark rm;
        tty->print("[ClassLoaderData: ");
        tty->print("create class loader data " INTPTR_FORMAT, p2i(cld));
        tty->print(" for instance " INTPTR_FORMAT " of %s", p2i((void *)cld->class_loader()),
                   cld->loader_name());
        tty->print_cr("]");
      }
      return cld;
    }
    next = exchanged;
  } while (true);

}

void ClassLoaderDataGraph::oops_do(OopClosure* f, KlassClosure* klass_closure, bool must_claim) {
  for (ClassLoaderData* cld = _head; cld != NULL; cld = cld->next()) {
    cld->oops_do(f, klass_closure, must_claim);
  }
}

void ClassLoaderDataGraph::keep_alive_oops_do(OopClosure* f, KlassClosure* klass_closure, bool must_claim) {
  for (ClassLoaderData* cld = _head; cld != NULL; cld = cld->next()) {
    if (cld->keep_alive()) {
      cld->oops_do(f, klass_closure, must_claim);
    }
  }
}

void ClassLoaderDataGraph::always_strong_oops_do(OopClosure* f, KlassClosure* klass_closure, bool must_claim) {
  if (ClassUnloading) {
    keep_alive_oops_do(f, klass_closure, must_claim);
  } else {
    oops_do(f, klass_closure, must_claim);
  }
}

void ClassLoaderDataGraph::cld_do(CLDClosure* cl) {
  for (ClassLoaderData* cld = _head; cl != NULL && cld != NULL; cld = cld->next()) {
    cl->do_cld(cld);
  }
}

void ClassLoaderDataGraph::roots_cld_do(CLDClosure* strong, CLDClosure* weak) {
  for (ClassLoaderData* cld = _head;  cld != NULL; cld = cld->_next) {
    CLDClosure* closure = cld->keep_alive() ? strong : weak;
    if (closure != NULL) {
      closure->do_cld(cld);
    }
  }
}

void ClassLoaderDataGraph::keep_alive_cld_do(CLDClosure* cl) {
  roots_cld_do(cl, NULL);
}

void ClassLoaderDataGraph::always_strong_cld_do(CLDClosure* cl) {
  if (ClassUnloading) {
    keep_alive_cld_do(cl);
  } else {
    cld_do(cl);
  }
}

void ClassLoaderDataGraph::classes_do(KlassClosure* klass_closure) {
  for (ClassLoaderData* cld = _head; cld != NULL; cld = cld->next()) {
    cld->classes_do(klass_closure);
  }
}

void ClassLoaderDataGraph::classes_do(void f(Klass* const)) {
  D_WARN_Unimplement;
}

void ClassLoaderDataGraph::loaded_classes_do(KlassClosure* klass_closure) {
  for (ClassLoaderData* cld = _head; cld != NULL; cld = cld->next()) {
    cld->loaded_classes_do(klass_closure);
  }
}

void ClassLoaderDataGraph::classes_unloading_do(void f(Klass* const)) {
  D_WARN_Unimplement;
}

GrowableArray<ClassLoaderData*>* ClassLoaderDataGraph::new_clds() {
  assert(_head == NULL || _saved_head != NULL, "remember_new_clds(true) not called?");

  GrowableArray<ClassLoaderData*>* array = new GrowableArray<ClassLoaderData*>();

  // The CLDs in [_head, _saved_head] were all added during last call to remember_new_clds(true);
  ClassLoaderData* curr = _head;
  while (curr != _saved_head) {
    if (!curr->claimed()) {
      array->push(curr);

      if (TraceClassLoaderData) {
        tty->print("[ClassLoaderData] found new CLD: ");
        curr->print_value_on(tty);
        tty->cr();
      }
    }

    curr = curr->_next;
  }

  return array;
}

bool ClassLoaderDataGraph::unload_list_contains(const void* x) {
  D_WARN_Unimplement;
}

#ifndef PRODUCT
bool ClassLoaderDataGraph::contains_loader_data(ClassLoaderData* loader_data) {
  for (ClassLoaderData* data = _head; data != NULL; data = data->next()) {
    if (loader_data == data) {
      return true;
    }
  }

  return false;
}
#endif // PRODUCT


// Move class loader data from main list to the unloaded list for unloading
// and deallocation later.
bool ClassLoaderDataGraph::do_unloading(BoolObjectClosure* is_alive_closure, bool clean_alive) {
  ClassLoaderData* data = _head;
  ClassLoaderData* prev = NULL;
  bool seen_dead_loader = false;

  // Save previous _unloading pointer for CMS which may add to unloading list before
  // purging and we don't want to rewalk the previously unloaded class loader data.
  _saved_unloading = _unloading;

  while (data != NULL) {
    if (data->is_alive(is_alive_closure)) {
      prev = data;
      data = data->next();
      continue;
    }
    seen_dead_loader = true;
    ClassLoaderData* dead = data;
    dead->unload();
    data = data->next();
    // Remove from loader list.
    // This class loader data will no longer be found
    // in the ClassLoaderDataGraph.
    if (prev != NULL) {
      prev->set_next(data);
    } else {
      assert(dead == _head, "sanity check");
      _head = data;
    }
    dead->set_next(_unloading);
    _unloading = dead;
  }

  if (clean_alive) {
    // Clean previous versions and the deallocate list.
    ClassLoaderDataGraph::clean_metaspaces();
  }

  if (seen_dead_loader) {
    post_class_unload_events();
  }

  return seen_dead_loader;
}

void ClassLoaderDataGraph::clean_metaspaces() {
  D_WARN_Unimplement;
}

void ClassLoaderDataGraph::purge() {
  D_WARN_Unimplement;
}

void ClassLoaderDataGraph::post_class_unload_events(void) {
  D_WARN_Unimplement;
}

void ClassLoaderDataGraph::free_deallocate_lists() {
  for (ClassLoaderData* cld = _head; cld != NULL; cld = cld->next()) {
    // We need to keep this data until InstanceKlass::purge_previous_version has been
    // called on all alive classes. See the comment in ClassLoaderDataGraph::clean_metaspaces.
    cld->free_deallocate_list();
  }

  // In some rare cases items added to the unloading list will not be freed elsewhere.
  // To keep it simple, walk the _unloading list also.
  for (ClassLoaderData* cld = _unloading; cld != _saved_unloading; cld = cld->next()) {
    cld->free_deallocate_list();
  }
}

// CDS support

// Global metaspaces for writing information to the shared archive.  When
// application CDS is supported, we may need one per metaspace, so this
// sort of looks like it.
static bool _shared_metaspaces_initialized = false;

// Initialize shared metaspaces (change to call from somewhere not lazily)
void ClassLoaderData::initialize_shared_metaspaces() {
  D_WARN_Unimplement;
}


void ClassLoaderData::print_value_on(outputStream* out) const {
  if (class_loader() == NULL) {
    out->print("NULL class_loader");
  } else {
    out->print("class loader " INTPTR_FORMAT, p2i(this));
    class_loader()->print_value_on(out);
  }
}
