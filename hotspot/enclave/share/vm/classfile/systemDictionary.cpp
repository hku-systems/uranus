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
#include "classfile/classLoaderData.inline.hpp"
//#include "classfile/dictionary.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/systemDictionary.hpp"
#if INCLUDE_CDS
#include "classfile/systemDictionaryShared.hpp"
#endif
#include "classfile/vmSymbols.hpp"
#include "interpreter/interpreter.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/klass.inline.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/oop.inline.hpp"
#include "oops/oop.inline2.hpp"
#include "oops/typeArrayKlass.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "runtime/signature.hpp"
#include "utilities/macros.hpp"

Dictionary*            SystemDictionary::_dictionary          = NULL;
PlaceholderTable*      SystemDictionary::_placeholders        = NULL;
Dictionary*            SystemDictionary::_shared_dictionary   = NULL;
LoaderConstraintTable* SystemDictionary::_loader_constraints  = NULL;
ResolutionErrorTable*  SystemDictionary::_resolution_errors   = NULL;
SymbolPropertyTable*   SystemDictionary::_invoke_method_table = NULL;


int         SystemDictionary::_number_of_modifications = 0;
int         SystemDictionary::_sdgeneration               = 0;
const int   SystemDictionary::_primelist[_prime_array_size] = {1009,2017,4049,5051,10103,
              20201,40423,99991};

oop         SystemDictionary::_system_loader_lock_obj     =  NULL;

Klass*      SystemDictionary::_well_known_klasses[SystemDictionary::WKID_LIMIT]
                                                          =  { NULL /*, NULL...*/ };
#ifndef ENCLAVE_UNIX
Klass*      SystemDictionary::_box_klasses[T_VOID+1]      =  { NULL /*, NULL...*/ };
#else
Klass**      SystemDictionary::_box_klasses;
#endif

oop         SystemDictionary::_java_system_loader         =  NULL;

bool        SystemDictionary::_has_loadClassInternal      =  false;
bool        SystemDictionary::_has_checkPackageAccess     =  false;

// lazily initialized klass variables
Klass* volatile SystemDictionary::_abstract_ownable_synchronizer_klass = NULL;


// ----------------------------------------------------------------------------
// Java-level SystemLoader

oop SystemDictionary::java_system_loader() {
  return _java_system_loader;
}

void SystemDictionary::compute_java_system_loader(TRAPS) {
  D_WARN_Unimplement;
}


ClassLoaderData* SystemDictionary::register_loader(Handle class_loader, TRAPS) {
  if (class_loader() == NULL) return ClassLoaderData::the_null_class_loader_data();
  return ClassLoaderDataGraph::find_or_create(class_loader, CHECK_NULL);
}

// ----------------------------------------------------------------------------
// debugging

#ifdef ASSERT

// return true if class_name contains no '.' (internal format is '/')
bool SystemDictionary::is_internal_format(Symbol* class_name) {
  if (class_name != NULL) {
    ResourceMark rm;
    char* name = class_name->as_C_string();
    return strchr(name, '.') == NULL;
  } else {
    return true;
  }
}

#endif

// ----------------------------------------------------------------------------
// Parallel class loading check

bool SystemDictionary::is_parallelCapable(Handle class_loader) {
  if (UnsyncloadClass || class_loader.is_null()) return true;
  if (AlwaysLockClassLoader) return false;
  return java_lang_ClassLoader::parallelCapable(class_loader());
}
// ----------------------------------------------------------------------------
// ParallelDefineClass flag does not apply to bootclass loader
bool SystemDictionary::is_parallelDefine(Handle class_loader) {
   if (class_loader.is_null()) return false;
   if (AllowParallelDefineClass && java_lang_ClassLoader::parallelCapable(class_loader())) {
     return true;
   }
   return false;
}

/**
 * Returns true if the passed class loader is the extension class loader.
 */
bool SystemDictionary::is_ext_class_loader(Handle class_loader) {
  if (class_loader.is_null()) {
    return false;
  }
  return (class_loader->klass()->name() == vmSymbols::sun_misc_Launcher_ExtClassLoader());
}

// ----------------------------------------------------------------------------
// Resolving of classes

// Forwards to resolve_or_null

Klass* SystemDictionary::resolve_or_fail(Symbol* class_name, Handle class_loader, Handle protection_domain, bool throw_error, TRAPS) {
  Klass* klass = resolve_or_null(class_name, class_loader, protection_domain, THREAD);
  if (HAS_PENDING_EXCEPTION || klass == NULL) {
    KlassHandle k_h(THREAD, klass);
    // can return a null klass
    klass = handle_resolution_exception(class_name, class_loader, protection_domain, throw_error, k_h, THREAD);
  }
  return klass;
}

Klass* SystemDictionary::handle_resolution_exception(Symbol* class_name, Handle class_loader, Handle protection_domain, bool throw_error, KlassHandle klass_h, TRAPS) {
  D_WARN_Unimplement;
}


Klass* SystemDictionary::resolve_or_fail(Symbol* class_name,
                                           bool throw_error, TRAPS)
{
  return resolve_or_fail(class_name, Handle(), Handle(), throw_error, THREAD);
}


// Forwards to resolve_instance_class_or_null

Klass* SystemDictionary::resolve_or_null(Symbol* class_name, Handle class_loader, Handle protection_domain, TRAPS) {
  D_WARN_Unimplement;
}

Klass* SystemDictionary::resolve_or_null(Symbol* class_name, TRAPS) {
  return resolve_or_null(class_name, Handle(), Handle(), THREAD);
}

// Forwards to resolve_instance_class_or_null

Klass* SystemDictionary::resolve_array_class_or_null(Symbol* class_name,
                                                       Handle class_loader,
                                                       Handle protection_domain,
                                                       TRAPS) {
  D_WARN_Unimplement;
}


// Must be called for any super-class or super-interface resolution
// during class definition to allow class circularity checking
// super-interface callers:
//    parse_interfaces - for defineClass & jvmtiRedefineClasses
// super-class callers:
//   ClassFileParser - for defineClass & jvmtiRedefineClasses
//   load_shared_class - while loading a class from shared archive
//   resolve_instance_class_or_null:
//     via: handle_parallel_super_load
//      when resolving a class that has an existing placeholder with
//      a saved superclass [i.e. a defineClass is currently in progress]
//      if another thread is trying to resolve the class, it must do
//      super-class checks on its own thread to catch class circularity
// This last call is critical in class circularity checking for cases
// where classloading is delegated to different threads and the
// classloader lock is released.
// Take the case: Base->Super->Base
//   1. If thread T1 tries to do a defineClass of class Base
//    resolve_super_or_fail creates placeholder: T1, Base (super Super)
//   2. resolve_instance_class_or_null does not find SD or placeholder for Super
//    so it tries to load Super
//   3. If we load the class internally, or user classloader uses same thread
//      loadClassFromxxx or defineClass via parseClassFile Super ...
//      3.1 resolve_super_or_fail creates placeholder: T1, Super (super Base)
//      3.3 resolve_instance_class_or_null Base, finds placeholder for Base
//      3.4 calls resolve_super_or_fail Base
//      3.5 finds T1,Base -> throws class circularity
//OR 4. If T2 tries to resolve Super via defineClass Super ...
//      4.1 resolve_super_or_fail creates placeholder: T2, Super (super Base)
//      4.2 resolve_instance_class_or_null Base, finds placeholder for Base (super Super)
//      4.3 calls resolve_super_or_fail Super in parallel on own thread T2
//      4.4 finds T2, Super -> throws class circularity
// Must be called, even if superclass is null, since this is
// where the placeholder entry is created which claims this
// thread is loading this class/classloader.
Klass* SystemDictionary::resolve_super_or_fail(Symbol* child_name,
                                                 Symbol* class_name,
                                                 Handle class_loader,
                                                 Handle protection_domain,
                                                 bool is_superclass,
                                                 TRAPS) {
  D_WARN_Unimplement;
}

void SystemDictionary::validate_protection_domain(instanceKlassHandle klass,
                                                  Handle class_loader,
                                                  Handle protection_domain,
                                                  TRAPS) {
  D_WARN_Unimplement;
}

// We only get here if this thread finds that another thread
// has already claimed the placeholder token for the current operation,
// but that other thread either never owned or gave up the
// object lock
// Waits on SystemDictionary_lock to indicate placeholder table updated
// On return, caller must recheck placeholder table state
//
// We only get here if
//  1) custom classLoader, i.e. not bootstrap classloader
//  2) UnsyncloadClass not set
//  3) custom classLoader has broken the class loader objectLock
//     so another thread got here in parallel
//
// lockObject must be held.
// Complicated dance due to lock ordering:
// Must first release the classloader object lock to
// allow initial definer to complete the class definition
// and to avoid deadlock
// Reclaim classloader lock object with same original recursion count
// Must release SystemDictionary_lock after notify, since
// class loader lock must be claimed before SystemDictionary_lock
// to prevent deadlocks
//
// The notify allows applications that did an untimed wait() on
// the classloader object lock to not hang.
void SystemDictionary::double_lock_wait(Handle lockObject, TRAPS) {
  D_WARN_Unimplement;
}

// If the class in is in the placeholder table, class loading is in progress
// For cases where the application changes threads to load classes, it
// is critical to ClassCircularity detection that we try loading
// the superclass on the same thread internally, so we do parallel
// super class loading here.
// This also is critical in cases where the original thread gets stalled
// even in non-circularity situations.
// Note: must call resolve_super_or_fail even if null super -
// to force placeholder entry creation for this class for circularity detection
// Caller must check for pending exception
// Returns non-null Klass* if other thread has completed load
// and we are done,
// If return null Klass* and no pending exception, the caller must load the class
instanceKlassHandle SystemDictionary::handle_parallel_super_load(
    Symbol* name, Symbol* superclassname, Handle class_loader,
    Handle protection_domain, Handle lockObject, TRAPS) {

  D_WARN_Unimplement;
}


Klass* SystemDictionary::resolve_instance_class_or_null(Symbol* name,
                                                        Handle class_loader,
                                                        Handle protection_domain,
                                                        TRAPS) {
  D_WARN_Unimplement;
}


// This routine does not lock the system dictionary.
//
// Since readers don't hold a lock, we must make sure that system
// dictionary entries are only removed at a safepoint (when only one
// thread is running), and are added to in a safe way (all links must
// be updated in an MT-safe manner).
//
// Callers should be aware that an entry could be added just after
// _dictionary->bucket(index) is read here, so the caller will not see
// the new entry.

Klass* SystemDictionary::find(Symbol* class_name,
                              Handle class_loader,
                              Handle protection_domain,
                              TRAPS) {

  // UseNewReflection
  // The result of this call should be consistent with the result
  // of the call to resolve_instance_class_or_null().
  // See evaluation 6790209 and 4474172 for more details.
  class_loader = Handle(THREAD, java_lang_ClassLoader::non_reflection_class_loader(class_loader()));
  ClassLoaderData* loader_data = ClassLoaderData::class_loader_data_or_null(class_loader());

  if (loader_data == NULL) {
    // If the ClassLoaderData has not been setup,
    // then the class loader has no entries in the dictionary.
    return NULL;
  }

  unsigned int d_hash = dictionary()->compute_hash(class_name, loader_data);
  int d_index = dictionary()->hash_to_index(d_hash);

  {
    // Note that we have an entry, and entries can be deleted only during GC,
    // so we cannot allow GC to occur while we're holding this entry.
    // We're using a No_Safepoint_Verifier to catch any place where we
    // might potentially do a GC at all.
    // Dictionary::do_unloading() asserts that classes in SD are only
    // unloaded at a safepoint. Anonymous classes are not in SD.
    return dictionary()->find(d_index, d_hash, class_name, loader_data,
                              protection_domain, THREAD);
  }
}


// Look for a loaded instance or array klass by name.  Do not do any loading.
// return NULL in case of error.
Klass* SystemDictionary::find_instance_or_array_klass(Symbol* class_name,
                                                      Handle class_loader,
                                                      Handle protection_domain,
                                                      TRAPS) {
  D_WARN_Unimplement;
}

// Note: this method is much like resolve_from_stream, but
// updates no supplemental data structures.
// TODO consolidate the two methods with a helper routine?
Klass* SystemDictionary::parse_stream(Symbol* class_name,
                                      Handle class_loader,
                                      Handle protection_domain,
                                      ClassFileStream* st,
                                      KlassHandle host_klass,
                                      GrowableArray<Handle>* cp_patches,
                                      TRAPS) {
  D_WARN_Unimplement;
}

// Add a klass to the system from a stream (called by jni_DefineClass and
// JVM_DefineClass).
// Note: class_name can be NULL. In that case we do not know the name of
// the class until we have parsed the stream.

Klass* SystemDictionary::resolve_from_stream(Symbol* class_name,
                                             Handle class_loader,
                                             Handle protection_domain,
                                             ClassFileStream* st,
                                             bool verify,
                                             TRAPS) {

  D_WARN_Unimplement;
}

#if INCLUDE_CDS
void SystemDictionary::set_shared_dictionary(HashtableBucket<mtClass>* t, int length,
                                             int number_of_entries) {
  assert(length == _nof_buckets * sizeof(HashtableBucket<mtClass>),
         "bad shared dictionary size.");
  _shared_dictionary = new Dictionary(_nof_buckets, t, number_of_entries);
}


// If there is a shared dictionary, then find the entry for the
// given shared system class, if any.

Klass* SystemDictionary::find_shared_class(Symbol* class_name) {
  if (shared_dictionary() != NULL) {
    unsigned int d_hash = shared_dictionary()->compute_hash(class_name, NULL);
    int d_index = shared_dictionary()->hash_to_index(d_hash);

    return shared_dictionary()->find_shared_class(d_index, d_hash, class_name);
  } else {
    return NULL;
  }
}


// Load a class from the shared spaces (found through the shared system
// dictionary).  Force the superclass and all interfaces to be loaded.
// Update the class definition to include sibling classes and no
// subclasses (yet).  [Classes in the shared space are not part of the
// object hierarchy until loaded.]

instanceKlassHandle SystemDictionary::load_shared_class(
                 Symbol* class_name, Handle class_loader, TRAPS) {
  D_WARN_Unimplement;
}

instanceKlassHandle SystemDictionary::load_shared_class(instanceKlassHandle ik,
                                                        Handle class_loader,
                                                        Handle protection_domain, TRAPS) {
  D_WARN_Unimplement;
}
#endif // INCLUDE_CDS

instanceKlassHandle SystemDictionary::load_instance_class(Symbol* class_name, Handle class_loader, TRAPS) {
  D_WARN_Unimplement;
}

void SystemDictionary::define_instance_class(instanceKlassHandle k, TRAPS) {
  D_WARN_Unimplement;
}

// Support parallel classloading
// All parallel class loaders, including bootstrap classloader
// lock a placeholder entry for this class/class_loader pair
// to allow parallel defines of different classes for this class loader
// With AllowParallelDefine flag==true, in case they do not synchronize around
// FindLoadedClass/DefineClass, calls, we check for parallel
// loading for them, wait if a defineClass is in progress
// and return the initial requestor's results
// This flag does not apply to the bootstrap classloader.
// With AllowParallelDefine flag==false, call through to define_instance_class
// which will throw LinkageError: duplicate class definition.
// False is the requested default.
// For better performance, the class loaders should synchronize
// findClass(), i.e. FindLoadedClass/DefineClassIfAbsent or they
// potentially waste time reading and parsing the bytestream.
// Note: VM callers should ensure consistency of k/class_name,class_loader
instanceKlassHandle SystemDictionary::find_or_define_instance_class(Symbol* class_name, Handle class_loader, instanceKlassHandle k, TRAPS) {

  D_WARN_Unimplement;
}
Handle SystemDictionary::compute_loader_lock_object(Handle class_loader, TRAPS) {
  // If class_loader is NULL we synchronize on _system_loader_lock_obj
  if (class_loader.is_null()) {
    return Handle(THREAD, _system_loader_lock_obj);
  } else {
    return class_loader;
  }
}

// This method is added to check how often we have to wait to grab loader
// lock. The results are being recorded in the performance counters defined in
// ClassLoader::_sync_systemLoaderLockContentionRate and
// ClassLoader::_sync_nonSystemLoaderLockConteionRate.
void SystemDictionary::check_loader_lock_contention(Handle loader_lock, TRAPS) {
  D_WARN_Unimplement;
}

// ----------------------------------------------------------------------------
// Lookup

Klass* SystemDictionary::find_class(int index, unsigned int hash,
                                      Symbol* class_name,
                                      ClassLoaderData* loader_data) {
  assert (index == dictionary()->index_for(class_name, loader_data),
          "incorrect index?");

  Klass* k = dictionary()->find_class(index, hash, class_name, loader_data);
  return k;
}


// Basic find on classes in the midst of being loaded
Symbol* SystemDictionary::find_placeholder(Symbol* class_name,
                                           ClassLoaderData* loader_data) {
  D_WARN_Unimplement;
}


// Used for assertions and verification only
Klass* SystemDictionary::find_class(Symbol* class_name, ClassLoaderData* loader_data) {
  // First look in the loaded class array
  unsigned int d_hash = dictionary()->compute_hash(class_name, loader_data);
  int d_index = dictionary()->hash_to_index(d_hash);
  return find_class(d_index, d_hash, class_name, loader_data);
}


// Get the next class in the diictionary.
Klass* SystemDictionary::try_get_next_class() {
  return dictionary()->try_get_next_class();
}


// ----------------------------------------------------------------------------
// Update hierachy. This is done before the new klass has been added to the SystemDictionary. The Recompile_lock
// is held, to ensure that the compiler is not using the class hierachy, and that deoptimization will kick in
// before a new class is used.

void SystemDictionary::add_to_hierarchy(instanceKlassHandle k, TRAPS) {
  D_WARN_Unimplement;
}

// ----------------------------------------------------------------------------
// GC support

// Following roots during mark-sweep is separated in two phases.
//
// The first phase follows preloaded classes and all other system
// classes, since these will never get unloaded anyway.
//
// The second phase removes (unloads) unreachable classes from the
// system dictionary and follows the remaining classes' contents.

void SystemDictionary::always_strong_oops_do(OopClosure* blk) {
  roots_oops_do(blk, NULL);
}

void SystemDictionary::always_strong_classes_do(KlassClosure* closure) {
  D_WARN_Unimplement;
}

// Calculate a "good" systemdictionary size based
// on predicted or current loaded classes count
int SystemDictionary::calculate_systemdictionary_size(int classcount) {
  int newsize = _old_default_sdsize;
  if ((classcount > 0)  && !DumpSharedSpaces) {
    int desiredsize = classcount/_average_depth_goal;
    for (newsize = _primelist[_sdgeneration]; _sdgeneration < _prime_array_size -1;
         newsize = _primelist[++_sdgeneration]) {
      if (desiredsize <=  newsize) {
        break;
      }
    }
  }
  return newsize;
}

#ifdef ASSERT
class VerifySDReachableAndLiveClosure : public OopClosure {
private:
  BoolObjectClosure* _is_alive;

  template <class T> void do_oop_work(T* p) {
    oop obj = oopDesc::load_decode_heap_oop(p);
    guarantee(_is_alive->do_object_b(obj), "Oop in system dictionary must be live");
  }

public:
  VerifySDReachableAndLiveClosure(BoolObjectClosure* is_alive) : OopClosure(), _is_alive(is_alive) { }

  virtual void do_oop(oop* p)       { do_oop_work(p); }
  virtual void do_oop(narrowOop* p) { do_oop_work(p); }
};
#endif

// Assumes classes in the SystemDictionary are only unloaded at a safepoint
// Note: anonymous classes are not in the SD.
bool SystemDictionary::do_unloading(BoolObjectClosure* is_alive, bool clean_alive) {
  D_WARN_Unimplement;
}

void SystemDictionary::roots_oops_do(OopClosure* strong, OopClosure* weak) {
  strong->do_oop(&_java_system_loader);
  strong->do_oop(&_system_loader_lock_obj);
  CDS_ONLY(SystemDictionaryShared::roots_oops_do(strong);)

  // Adjust dictionary
  dictionary()->roots_oops_do(strong, weak);

  // Visit extra methods
  invoke_method_table()->oops_do(strong);
}

void SystemDictionary::oops_do(OopClosure* f) {
  D_WARN_Unimplement;
}

// Extended Class redefinition support.
// If one of these classes is replaced, we need to replace it in these places.
// KlassClosure::do_klass should take the address of a class but we can
// change that later.
void SystemDictionary::preloaded_classes_do(KlassClosure* f) {
  D_WARN_Unimplement;
}

void SystemDictionary::lazily_loaded_classes_do(KlassClosure* f) {
  f->do_klass(_abstract_ownable_synchronizer_klass);
}

// Just the classes from defining class loaders
// Don't iterate over placeholders
void SystemDictionary::classes_do(void f(Klass*)) {
  dictionary()->classes_do(f);
}

// Added for initialize_itable_for_klass
//   Just the classes from defining class loaders
// Don't iterate over placeholders
void SystemDictionary::classes_do(void f(Klass*, TRAPS), TRAPS) {
  dictionary()->classes_do(f, CHECK);
}

//   All classes, and their class loaders
// Don't iterate over placeholders
void SystemDictionary::classes_do(void f(Klass*, ClassLoaderData*)) {
  D_WARN_Unimplement;
}

void SystemDictionary::placeholders_do(void f(Symbol*)) {
  D_WARN_Unimplement;
}

void SystemDictionary::methods_do(void f(Method*)) {
  D_WARN_Unimplement;
}

void SystemDictionary::remove_classes_in_error_state() {
  dictionary()->remove_classes_in_error_state();
}

// ----------------------------------------------------------------------------
// Lazily load klasses

void SystemDictionary::load_abstract_ownable_synchronizer_klass(TRAPS) {
  assert(JDK_Version::is_gte_jdk16x_version(), "Must be JDK 1.6 or later");

  // if multiple threads calling this function, only one thread will load
  // the class.  The other threads will find the loaded version once the
  // class is loaded.
  Klass* aos = _abstract_ownable_synchronizer_klass;
  if (aos == NULL) {
    Klass* k = resolve_or_fail(vmSymbols::java_util_concurrent_locks_AbstractOwnableSynchronizer(), true, CHECK);
    // Force a fence to prevent any read before the write completes
    OrderAccess::fence();
    _abstract_ownable_synchronizer_klass = k;
  }
}

// ----------------------------------------------------------------------------
// Initialization

void SystemDictionary::initialize(TRAPS) {
  D_WARN_Unimplement;
}

// Compact table of directions on the initialization of klasses:
static const short wk_init_info[] = {
  #define WK_KLASS_INIT_INFO(name, symbol, option) \
    ( ((int)vmSymbols::VM_SYMBOL_ENUM_NAME(symbol) \
          << SystemDictionary::CEIL_LG_OPTION_LIMIT) \
      | (int)SystemDictionary::option ),
  WK_KLASSES_DO(WK_KLASS_INIT_INFO)
  #undef WK_KLASS_INIT_INFO
  0
};

bool SystemDictionary::initialize_wk_klass(WKID id, int init_opt, TRAPS) {
  assert(id >= (int)FIRST_WKID && id < (int)WKID_LIMIT, "oob");
  int  info = wk_init_info[id - FIRST_WKID];
  int  sid  = (info >> CEIL_LG_OPTION_LIMIT);
  Symbol* symbol = vmSymbols::symbol_at((vmSymbols::SID)sid);
  Klass**    klassp = &_well_known_klasses[id];
  bool must_load = (init_opt < SystemDictionary::Opt);
  if ((*klassp) == NULL) {
    if (must_load) {
      (*klassp) = resolve_or_fail(symbol, true, CHECK_0); // load required class
    } else {
      (*klassp) = resolve_or_null(symbol,       CHECK_0); // load optional klass
    }
  }
  return ((*klassp) != NULL);
}

void SystemDictionary::initialize_wk_klasses_until(WKID limit_id, WKID &start_id, TRAPS) {
  assert((int)start_id <= (int)limit_id, "IDs are out of order!");
  for (int id = (int)start_id; id < (int)limit_id; id++) {
    assert(id >= (int)FIRST_WKID && id < (int)WKID_LIMIT, "oob");
    int info = wk_init_info[id - FIRST_WKID];
    int sid  = (info >> CEIL_LG_OPTION_LIMIT);
    int opt  = (info & right_n_bits(CEIL_LG_OPTION_LIMIT));

    initialize_wk_klass((WKID)id, opt, CHECK);
  }

  // move the starting value forward to the limit:
  start_id = limit_id;
}

void SystemDictionary::initialize_preloaded_classes(TRAPS) {
  D_WARN_Unimplement;
}

// Tells if a given klass is a box (wrapper class, such as java.lang.Integer).
// If so, returns the basic type it holds.  If not, returns T_OBJECT.
BasicType SystemDictionary::box_klass_type(Klass* k) {
  assert(k != NULL, "");
  for (int i = T_BOOLEAN; i < T_VOID+1; i++) {
    if (_box_klasses[i] == k)
      return (BasicType)i;
  }
  return T_OBJECT;
}

// Constraints on class loaders. The details of the algorithm can be
// found in the OOPSLA'98 paper "Dynamic Class Loading in the Java
// Virtual Machine" by Sheng Liang and Gilad Bracha.  The basic idea is
// that the system dictionary needs to maintain a set of contraints that
// must be satisfied by all classes in the dictionary.
// if defining is true, then LinkageError if already in systemDictionary
// if initiating loader, then ok if InstanceKlass matches existing entry

void SystemDictionary::check_constraints(int d_index, unsigned int d_hash,
                                         instanceKlassHandle k,
                                         Handle class_loader, bool defining,
                                         TRAPS) {
  D_WARN_Unimplement;
}


// Update system dictionary - done after check_constraint and add_to_hierachy
// have been called.
void SystemDictionary::update_dictionary(int d_index, unsigned int d_hash,
                                         int p_index, unsigned int p_hash,
                                         instanceKlassHandle k,
                                         Handle class_loader,
                                         TRAPS) {
  D_WARN_Unimplement;
}


// Try to find a class name using the loader constraints.  The
// loader constraints might know about a class that isn't fully loaded
// yet and these will be ignored.
Klass* SystemDictionary::find_constrained_instance_or_array_klass(
                    Symbol* class_name, Handle class_loader, TRAPS) {

  D_WARN_Unimplement;
}


bool SystemDictionary::add_loader_constraint(Symbol* class_name,
                                             Handle class_loader1,
                                             Handle class_loader2,
                                             Thread* THREAD) {
  D_WARN_Unimplement;
}

// Add entry to resolution error table to record the error when the first
// attempt to resolve a reference to a class has failed.
void SystemDictionary::add_resolution_error(constantPoolHandle pool, int which, Symbol* error) {
  D_WARN_Unimplement;
}

// Delete a resolution error for RedefineClasses for a constant pool is going away
void SystemDictionary::delete_resolution_error(ConstantPool* pool) {
  D_WARN_Unimplement;
}

// Lookup resolution error table. Returns error if found, otherwise NULL.
Symbol* SystemDictionary::find_resolution_error(constantPoolHandle pool, int which) {
  D_WARN_Unimplement;
}


// Signature constraints ensure that callers and callees agree about
// the meaning of type names in their signatures.  This routine is the
// intake for constraints.  It collects them from several places:
//
//  * LinkResolver::resolve_method (if check_access is true) requires
//    that the resolving class (the caller) and the defining class of
//    the resolved method (the callee) agree on each type in the
//    method's signature.
//
//  * LinkResolver::resolve_interface_method performs exactly the same
//    checks.
//
//  * LinkResolver::resolve_field requires that the constant pool
//    attempting to link to a field agree with the field's defining
//    class about the type of the field signature.
//
//  * klassVtable::initialize_vtable requires that, when a class
//    overrides a vtable entry allocated by a superclass, that the
//    overriding method (i.e., the callee) agree with the superclass
//    on each type in the method's signature.
//
//  * klassItable::initialize_itable requires that, when a class fills
//    in its itables, for each non-abstract method installed in an
//    itable, the method (i.e., the callee) agree with the interface
//    on each type in the method's signature.
//
// All those methods have a boolean (check_access, checkconstraints)
// which turns off the checks.  This is used from specialized contexts
// such as bootstrapping, dumping, and debugging.
//
// No direct constraint is placed between the class and its
// supertypes.  Constraints are only placed along linked relations
// between callers and callees.  When a method overrides or implements
// an abstract method in a supertype (superclass or interface), the
// constraints are placed as if the supertype were the caller to the
// overriding method.  (This works well, since callers to the
// supertype have already established agreement between themselves and
// the supertype.)  As a result of all this, a class can disagree with
// its supertype about the meaning of a type name, as long as that
// class neither calls a relevant method of the supertype, nor is
// called (perhaps via an override) from the supertype.
//
//
// SystemDictionary::check_signature_loaders(sig, l1, l2)
//
// Make sure all class components (including arrays) in the given
// signature will be resolved to the same class in both loaders.
// Returns the name of the type that failed a loader constraint check, or
// NULL if no constraint failed.  No exception except OOME is thrown.
// Arrays are not added to the loader constraint table, their elements are.
Symbol* SystemDictionary::check_signature_loaders(Symbol* signature,
                                               Handle loader1, Handle loader2,
                                               bool is_method, TRAPS)  {
  // Nothing to do if loaders are the same.
  if (loader1() == loader2()) {
    return NULL;
  }

  SignatureStream sig_strm(signature, is_method);
  while (!sig_strm.is_done()) {
    if (sig_strm.is_object()) {
      Symbol* sig = sig_strm.as_symbol(CHECK_NULL);
      if (!add_loader_constraint(sig, loader1, loader2, THREAD)) {
        return sig;
      }
    }
    sig_strm.next();
  }
  return NULL;
}


methodHandle SystemDictionary::find_method_handle_intrinsic(vmIntrinsics::ID iid,
                                                            Symbol* signature,
                                                            TRAPS) {
  D_WARN_Unimplement;
}

// Helper for unpacking the return value from linkMethod and linkCallSite.
static methodHandle unpack_method_and_appendix(Handle mname,
                                               KlassHandle accessing_klass,
                                               objArrayHandle appendix_box,
                                               Handle* appendix_result,
                                               TRAPS) {
  D_WARN_Unimplement;
}

methodHandle SystemDictionary::find_method_handle_invoker(Symbol* name,
                                                          Symbol* signature,
                                                          KlassHandle accessing_klass,
                                                          Handle *appendix_result,
                                                          Handle *method_type_result,
                                                          TRAPS) {
  D_WARN_Unimplement;
}

// Decide if we can globally cache a lookup of this class, to be returned to any client that asks.
// We must ensure that all class loaders everywhere will reach this class, for any client.
// This is a safe bet for public classes in java.lang, such as Object and String.
// We also include public classes in java.lang.invoke, because they appear frequently in system-level method types.
// Out of an abundance of caution, we do not include any other classes, not even for packages like java.util.
static bool is_always_visible_class(oop mirror) {
  D_WARN_Unimplement;
}

// Ask Java code to find or construct a java.lang.invoke.MethodType for the given
// signature, as interpreted relative to the given class loader.
// Because of class loader constraints, all method handle usage must be
// consistent with this loader.
Handle SystemDictionary::find_method_handle_type(Symbol* signature,
                                                 KlassHandle accessing_klass,
                                                 TRAPS) {
  D_WARN_Unimplement;
}

// Ask Java code to find or construct a method handle constant.
Handle SystemDictionary::link_method_handle_constant(KlassHandle caller,
                                                     int ref_kind, //e.g., JVM_REF_invokeVirtual
                                                     KlassHandle callee,
                                                     Symbol* name_sym,
                                                     Symbol* signature,
                                                     TRAPS) {
  D_WARN_Unimplement;
}

// Ask Java code to find or construct a java.lang.invoke.CallSite for the given
// name and signature, as interpreted relative to the given class loader.
methodHandle SystemDictionary::find_dynamic_call_site_invoker(KlassHandle caller,
                                                              Handle bootstrap_specifier,
                                                              Symbol* name,
                                                              Symbol* type,
                                                              Handle *appendix_result,
                                                              Handle *method_type_result,
                                                              TRAPS) {
  D_WARN_Unimplement;
}

// Since the identity hash code for symbols changes when the symbols are
// moved from the regular perm gen (hash in the mark word) to the shared
// spaces (hash is the address), the classes loaded into the dictionary
// may be in the wrong buckets.

void SystemDictionary::reorder_dictionary() {
  dictionary()->reorder_dictionary();
}


void SystemDictionary::copy_buckets(char** top, char* end) {
  dictionary()->copy_buckets(top, end);
}


void SystemDictionary::copy_table(char** top, char* end) {
  dictionary()->copy_table(top, end);
}


void SystemDictionary::reverse() {
  dictionary()->reverse();
}

int SystemDictionary::number_of_classes() {
  return dictionary()->number_of_entries();
}


// ----------------------------------------------------------------------------
void SystemDictionary::print_shared(bool details) {
  shared_dictionary()->print(details);
}

void SystemDictionary::print(bool details) {
  D_WARN_Unimplement;
}


void SystemDictionary::verify() {
  D_WARN_Unimplement;
}

// utility function for class load event
void SystemDictionary::post_class_load_event(const Ticks& start_time,
                                             instanceKlassHandle k,
                                             Handle initiating_loader) {

}

#ifndef PRODUCT

// statistics code
class ClassStatistics: AllStatic {
 private:
  static int nclasses;        // number of classes
  static int nmethods;        // number of methods
  static int nmethoddata;     // number of methodData
  static int class_size;      // size of class objects in words
  static int method_size;     // size of method objects in words
  static int debug_size;      // size of debug info in methods
  static int methoddata_size; // size of methodData objects in words

  static void do_class(Klass* k) {
    nclasses++;
    class_size += k->size();
    if (k->oop_is_instance()) {
      InstanceKlass* ik = (InstanceKlass*)k;
      class_size += ik->methods()->size();
      class_size += ik->constants()->size();
      class_size += ik->local_interfaces()->size();
      class_size += ik->transitive_interfaces()->size();
      // We do not have to count implementors, since we only store one!
      // SSS: How should these be accounted now that they have moved?
      // class_size += ik->fields()->length();
    }
  }

  static void do_method(Method* m) {
    nmethods++;
    method_size += m->size();
    // class loader uses same objArray for empty vectors, so don't count these
    if (m->has_stackmap_table()) {
      method_size += m->stackmap_data()->size();
    }

    MethodData* mdo = m->method_data();
    if (mdo != NULL) {
      nmethoddata++;
      methoddata_size += mdo->size();
    }
  }

 public:
  static void print() {
    SystemDictionary::classes_do(do_class);
    SystemDictionary::methods_do(do_method);
    tty->print_cr("Class statistics:");
    tty->print_cr("%d classes (%d bytes)", nclasses, class_size * oopSize);
    tty->print_cr("%d methods (%d bytes = %d base + %d debug info)", nmethods,
                  (method_size + debug_size) * oopSize, method_size * oopSize, debug_size * oopSize);
    tty->print_cr("%d methoddata (%d bytes)", nmethoddata, methoddata_size * oopSize);
  }
};


int ClassStatistics::nclasses        = 0;
int ClassStatistics::nmethods        = 0;
int ClassStatistics::nmethoddata     = 0;
int ClassStatistics::class_size      = 0;
int ClassStatistics::method_size     = 0;
int ClassStatistics::debug_size      = 0;
int ClassStatistics::methoddata_size = 0;

void SystemDictionary::print_class_statistics() {
  ResourceMark rm;
  ClassStatistics::print();
}


class MethodStatistics: AllStatic {
 public:
  enum {
    max_parameter_size = 10
  };
 private:

  static int _number_of_methods;
  static int _number_of_final_methods;
  static int _number_of_static_methods;
  static int _number_of_native_methods;
  static int _number_of_synchronized_methods;
  static int _number_of_profiled_methods;
  static int _number_of_bytecodes;
  static int _parameter_size_profile[max_parameter_size];
  static int _bytecodes_profile[Bytecodes::number_of_java_codes];

  static void initialize() {
    _number_of_methods        = 0;
    _number_of_final_methods  = 0;
    _number_of_static_methods = 0;
    _number_of_native_methods = 0;
    _number_of_synchronized_methods = 0;
    _number_of_profiled_methods = 0;
    _number_of_bytecodes      = 0;
    for (int i = 0; i < max_parameter_size             ; i++) _parameter_size_profile[i] = 0;
    for (int j = 0; j < Bytecodes::number_of_java_codes; j++) _bytecodes_profile     [j] = 0;
  };

  static void do_method(Method* m) {
    _number_of_methods++;
    // collect flag info
    if (m->is_final()       ) _number_of_final_methods++;
    if (m->is_static()      ) _number_of_static_methods++;
    if (m->is_native()      ) _number_of_native_methods++;
    if (m->is_synchronized()) _number_of_synchronized_methods++;
    if (m->method_data() != NULL) _number_of_profiled_methods++;
    // collect parameter size info (add one for receiver, if any)
    _parameter_size_profile[MIN2(m->size_of_parameters() + (m->is_static() ? 0 : 1), max_parameter_size - 1)]++;
    // collect bytecodes info
    {
      Thread *thread = Thread::current();
      HandleMark hm(thread);
      BytecodeStream s(methodHandle(thread, m));
      Bytecodes::Code c;
      while ((c = s.next()) >= 0) {
        _number_of_bytecodes++;
        _bytecodes_profile[c]++;
      }
    }
  }

 public:
  static void print() {
    initialize();
    SystemDictionary::methods_do(do_method);
    // generate output
    tty->cr();
    tty->print_cr("Method statistics (static):");
    // flag distribution
    tty->cr();
    tty->print_cr("%6d final        methods  %6.1f%%", _number_of_final_methods       , _number_of_final_methods        * 100.0F / _number_of_methods);
    tty->print_cr("%6d static       methods  %6.1f%%", _number_of_static_methods      , _number_of_static_methods       * 100.0F / _number_of_methods);
    tty->print_cr("%6d native       methods  %6.1f%%", _number_of_native_methods      , _number_of_native_methods       * 100.0F / _number_of_methods);
    tty->print_cr("%6d synchronized methods  %6.1f%%", _number_of_synchronized_methods, _number_of_synchronized_methods * 100.0F / _number_of_methods);
    tty->print_cr("%6d profiled     methods  %6.1f%%", _number_of_profiled_methods, _number_of_profiled_methods * 100.0F / _number_of_methods);
    // parameter size profile
    tty->cr();
    { int tot = 0;
      int avg = 0;
      for (int i = 0; i < max_parameter_size; i++) {
        int n = _parameter_size_profile[i];
        tot += n;
        avg += n*i;
        tty->print_cr("parameter size = %1d: %6d methods  %5.1f%%", i, n, n * 100.0F / _number_of_methods);
      }
      assert(tot == _number_of_methods, "should be the same");
      tty->print_cr("                    %6d methods  100.0%%", _number_of_methods);
      tty->print_cr("(average parameter size = %3.1f including receiver, if any)", (float)avg / _number_of_methods);
    }
    // bytecodes profile
    tty->cr();
    { int tot = 0;
      for (int i = 0; i < Bytecodes::number_of_java_codes; i++) {
        if (Bytecodes::is_defined(i)) {
          Bytecodes::Code c = Bytecodes::cast(i);
          int n = _bytecodes_profile[c];
          tot += n;
          tty->print_cr("%9d  %7.3f%%  %s", n, n * 100.0F / _number_of_bytecodes, Bytecodes::name(c));
        }
      }
      assert(tot == _number_of_bytecodes, "should be the same");
      tty->print_cr("%9d  100.000%%", _number_of_bytecodes);
    }
    tty->cr();
  }
};

int MethodStatistics::_number_of_methods;
int MethodStatistics::_number_of_final_methods;
int MethodStatistics::_number_of_static_methods;
int MethodStatistics::_number_of_native_methods;
int MethodStatistics::_number_of_synchronized_methods;
int MethodStatistics::_number_of_profiled_methods;
int MethodStatistics::_number_of_bytecodes;
int MethodStatistics::_parameter_size_profile[MethodStatistics::max_parameter_size];
int MethodStatistics::_bytecodes_profile[Bytecodes::number_of_java_codes];


void SystemDictionary::print_method_statistics() {
  MethodStatistics::print();
}

#endif // PRODUCT
