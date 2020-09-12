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
#include "classfile/classLoaderData.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "interpreter/interpreter.hpp"
#include "memory/oopFactory.hpp"
#include "memory/universe.hpp"
#include "memory/universe.inline.hpp"
#include "oops/constantPool.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/instanceMirrorKlass.hpp"
#include "oops/oop.inline.hpp"
#include "oops/typeArrayKlass.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/synchronizer.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/copy.hpp"
#include "utilities/hashtable.inline.hpp"
#include "utilities/macros.hpp"

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

// Known objects
Klass* Universe::_boolArrayKlassObj                 = NULL;
Klass* Universe::_byteArrayKlassObj                 = NULL;
Klass* Universe::_charArrayKlassObj                 = NULL;
Klass* Universe::_intArrayKlassObj                  = NULL;
Klass* Universe::_shortArrayKlassObj                = NULL;
Klass* Universe::_longArrayKlassObj                 = NULL;
Klass* Universe::_singleArrayKlassObj               = NULL;
Klass* Universe::_doubleArrayKlassObj               = NULL;
Klass* Universe::_typeArrayKlassObjs[T_VOID+1]      = { NULL /*, NULL...*/ };
Klass* Universe::_objectArrayKlassObj               = NULL;
oop Universe::_int_mirror                             = NULL;
oop Universe::_float_mirror                           = NULL;
oop Universe::_double_mirror                          = NULL;
oop Universe::_byte_mirror                            = NULL;
oop Universe::_bool_mirror                            = NULL;
oop Universe::_char_mirror                            = NULL;
oop Universe::_long_mirror                            = NULL;
oop Universe::_short_mirror                           = NULL;
oop Universe::_void_mirror                            = NULL;
oop Universe::_mirrors[T_VOID+1]                      = { NULL /*, NULL...*/ };
oop Universe::_main_thread_group                      = NULL;
oop Universe::_system_thread_group                    = NULL;
objArrayOop Universe::_the_empty_class_klass_array    = NULL;
Array<Klass*>* Universe::_the_array_interfaces_array = NULL;
oop Universe::_the_null_string                        = NULL;
oop Universe::_the_min_jint_string                   = NULL;
LatestMethodCache* Universe::_finalizer_register_cache = NULL;
LatestMethodCache* Universe::_loader_addClass_cache    = NULL;
LatestMethodCache* Universe::_pd_implies_cache         = NULL;
LatestMethodCache* Universe::_throw_illegal_access_error_cache = NULL;
oop Universe::_out_of_memory_error_java_heap          = NULL;
oop Universe::_out_of_memory_error_metaspace          = NULL;
oop Universe::_out_of_memory_error_class_metaspace    = NULL;
oop Universe::_out_of_memory_error_array_size         = NULL;
oop Universe::_out_of_memory_error_gc_overhead_limit  = NULL;
oop Universe::_out_of_memory_error_realloc_objects    = NULL;
objArrayOop Universe::_preallocated_out_of_memory_error_array = NULL;
volatile jint Universe::_preallocated_out_of_memory_error_avail_count = 0;
bool Universe::_verify_in_progress                    = false;
long Universe::verify_flags                           = Universe::Verify_All;
oop Universe::_null_ptr_exception_instance            = NULL;
oop Universe::_arithmetic_exception_instance          = NULL;
oop Universe::_virtual_machine_error_instance         = NULL;
oop Universe::_vm_exception                           = NULL;
oop Universe::_allocation_context_notification_obj    = NULL;

Array<int>* Universe::_the_empty_int_array            = NULL;
Array<u2>* Universe::_the_empty_short_array           = NULL;
Array<Klass*>* Universe::_the_empty_klass_array     = NULL;
Array<Method*>* Universe::_the_empty_method_array   = NULL;

// These variables are guarded by FullGCALot_lock.
debug_only(objArrayOop Universe::_fullgc_alot_dummy_array = NULL;)
debug_only(int Universe::_fullgc_alot_dummy_next      = 0;)

// Heap
int             Universe::_verify_count = 0;

int             Universe::_base_vtable_size = 0;
bool            Universe::_bootstrapping = false;
bool            Universe::_fully_initialized = false;

size_t          Universe::_heap_capacity_at_last_gc;
size_t          Universe::_heap_used_at_last_gc = 0;

CollectedHeap*  Universe::_collectedHeap = NULL;

NarrowPtrStruct Universe::_narrow_oop = { NULL, 0, true };
NarrowPtrStruct Universe::_narrow_klass = { NULL, 0, true };
address Universe::_narrow_ptrs_base;

void Universe::basic_type_classes_do(void f(Klass*)) {
  f(boolArrayKlassObj());
  f(byteArrayKlassObj());
  f(charArrayKlassObj());
  f(intArrayKlassObj());
  f(shortArrayKlassObj());
  f(longArrayKlassObj());
  f(singleArrayKlassObj());
  f(doubleArrayKlassObj());
}

void Universe::oops_do(OopClosure* f, bool do_all) {

  f->do_oop((oop*) &_int_mirror);
  f->do_oop((oop*) &_float_mirror);
  f->do_oop((oop*) &_double_mirror);
  f->do_oop((oop*) &_byte_mirror);
  f->do_oop((oop*) &_bool_mirror);
  f->do_oop((oop*) &_char_mirror);
  f->do_oop((oop*) &_long_mirror);
  f->do_oop((oop*) &_short_mirror);
  f->do_oop((oop*) &_void_mirror);

  for (int i = T_BOOLEAN; i < T_VOID+1; i++) {
    f->do_oop((oop*) &_mirrors[i]);
  }
  assert(_mirrors[0] == NULL && _mirrors[T_BOOLEAN - 1] == NULL, "checking");

  f->do_oop((oop*)&_the_empty_class_klass_array);
  f->do_oop((oop*)&_the_null_string);
  f->do_oop((oop*)&_the_min_jint_string);
  f->do_oop((oop*)&_out_of_memory_error_java_heap);
  f->do_oop((oop*)&_out_of_memory_error_metaspace);
  f->do_oop((oop*)&_out_of_memory_error_class_metaspace);
  f->do_oop((oop*)&_out_of_memory_error_array_size);
  f->do_oop((oop*)&_out_of_memory_error_gc_overhead_limit);
  f->do_oop((oop*)&_out_of_memory_error_realloc_objects);
    f->do_oop((oop*)&_preallocated_out_of_memory_error_array);
  f->do_oop((oop*)&_null_ptr_exception_instance);
  f->do_oop((oop*)&_arithmetic_exception_instance);
  f->do_oop((oop*)&_virtual_machine_error_instance);
  f->do_oop((oop*)&_main_thread_group);
  f->do_oop((oop*)&_system_thread_group);
  f->do_oop((oop*)&_vm_exception);
  f->do_oop((oop*)&_allocation_context_notification_obj);
  debug_only(f->do_oop((oop*)&_fullgc_alot_dummy_array);)
}

// Serialize metadata in and out of CDS archive, not oops.
void Universe::serialize(SerializeClosure* f, bool do_all) {

  f->do_ptr((void**)&_boolArrayKlassObj);
  f->do_ptr((void**)&_byteArrayKlassObj);
  f->do_ptr((void**)&_charArrayKlassObj);
  f->do_ptr((void**)&_intArrayKlassObj);
  f->do_ptr((void**)&_shortArrayKlassObj);
  f->do_ptr((void**)&_longArrayKlassObj);
  f->do_ptr((void**)&_singleArrayKlassObj);
  f->do_ptr((void**)&_doubleArrayKlassObj);
  f->do_ptr((void**)&_objectArrayKlassObj);

  {
    for (int i = 0; i < T_VOID+1; i++) {
      if (_typeArrayKlassObjs[i] != NULL) {
        assert(i >= T_BOOLEAN, "checking");
        f->do_ptr((void**)&_typeArrayKlassObjs[i]);
      } else if (do_all) {
        f->do_ptr((void**)&_typeArrayKlassObjs[i]);
      }
    }
  }

  f->do_ptr((void**)&_the_array_interfaces_array);
  f->do_ptr((void**)&_the_empty_int_array);
  f->do_ptr((void**)&_the_empty_short_array);
  f->do_ptr((void**)&_the_empty_method_array);
  f->do_ptr((void**)&_the_empty_klass_array);
  _finalizer_register_cache->serialize(f);
  _loader_addClass_cache->serialize(f);
  _pd_implies_cache->serialize(f);
  _throw_illegal_access_error_cache->serialize(f);
}

void Universe::check_alignment(uintx size, uintx alignment, const char* name) {
  D_WARN_Unimplement;
}

void initialize_basic_type_klass(Klass* k, TRAPS) {
  Klass* ok = SystemDictionary::Object_klass();
  if (UseSharedSpaces) {
    ClassLoaderData* loader_data = ClassLoaderData::the_null_class_loader_data();
    assert(k->super() == ok, "u3");
    k->restore_unshareable_info(loader_data, Handle(), CHECK);
  } else {
    k->initialize_supers(ok, CHECK);
  }
  k->append_to_sibling_list();
}

void Universe::genesis(TRAPS) {
  ResourceMark rm;

  { FlagSetting fs(_bootstrapping, true);

    { MutexLocker mc(Compile_lock);

      // determine base vtable size; without that we cannot create the array klasses
      compute_base_vtable_size();

      if (!UseSharedSpaces) {
        _boolArrayKlassObj      = TypeArrayKlass::create_klass(T_BOOLEAN, sizeof(jboolean), CHECK);
        _charArrayKlassObj      = TypeArrayKlass::create_klass(T_CHAR,    sizeof(jchar),    CHECK);
        _singleArrayKlassObj    = TypeArrayKlass::create_klass(T_FLOAT,   sizeof(jfloat),   CHECK);
        _doubleArrayKlassObj    = TypeArrayKlass::create_klass(T_DOUBLE,  sizeof(jdouble),  CHECK);
        _byteArrayKlassObj      = TypeArrayKlass::create_klass(T_BYTE,    sizeof(jbyte),    CHECK);
        _shortArrayKlassObj     = TypeArrayKlass::create_klass(T_SHORT,   sizeof(jshort),   CHECK);
        _intArrayKlassObj       = TypeArrayKlass::create_klass(T_INT,     sizeof(jint),     CHECK);
        _longArrayKlassObj      = TypeArrayKlass::create_klass(T_LONG,    sizeof(jlong),    CHECK);

        _typeArrayKlassObjs[T_BOOLEAN] = _boolArrayKlassObj;
        _typeArrayKlassObjs[T_CHAR]    = _charArrayKlassObj;
        _typeArrayKlassObjs[T_FLOAT]   = _singleArrayKlassObj;
        _typeArrayKlassObjs[T_DOUBLE]  = _doubleArrayKlassObj;
        _typeArrayKlassObjs[T_BYTE]    = _byteArrayKlassObj;
        _typeArrayKlassObjs[T_SHORT]   = _shortArrayKlassObj;
        _typeArrayKlassObjs[T_INT]     = _intArrayKlassObj;
        _typeArrayKlassObjs[T_LONG]    = _longArrayKlassObj;

        ClassLoaderData* null_cld = ClassLoaderData::the_null_class_loader_data();

        _the_array_interfaces_array = MetadataFactory::new_array<Klass*>(null_cld, 2, NULL, CHECK);
        _the_empty_int_array        = MetadataFactory::new_array<int>(null_cld, 0, CHECK);
        _the_empty_short_array      = MetadataFactory::new_array<u2>(null_cld, 0, CHECK);
        _the_empty_method_array     = MetadataFactory::new_array<Method*>(null_cld, 0, CHECK);
        _the_empty_klass_array      = MetadataFactory::new_array<Klass*>(null_cld, 0, CHECK);
      }
    }

    vmSymbols::initialize(CHECK);

    SystemDictionary::initialize(CHECK);

    Klass* ok = SystemDictionary::Object_klass();

    _the_null_string            = StringTable::intern("null", CHECK);
    _the_min_jint_string       = StringTable::intern("-2147483648", CHECK);

    if (UseSharedSpaces) {
      // Verify shared interfaces array.
      assert(_the_array_interfaces_array->at(0) ==
             SystemDictionary::Cloneable_klass(), "u3");
      assert(_the_array_interfaces_array->at(1) ==
             SystemDictionary::Serializable_klass(), "u3");
    } else {
      // Set up shared interfaces array.  (Do this before supers are set up.)
      _the_array_interfaces_array->at_put(0, SystemDictionary::Cloneable_klass());
      _the_array_interfaces_array->at_put(1, SystemDictionary::Serializable_klass());
    }

    initialize_basic_type_klass(boolArrayKlassObj(), CHECK);
    initialize_basic_type_klass(charArrayKlassObj(), CHECK);
    initialize_basic_type_klass(singleArrayKlassObj(), CHECK);
    initialize_basic_type_klass(doubleArrayKlassObj(), CHECK);
    initialize_basic_type_klass(byteArrayKlassObj(), CHECK);
    initialize_basic_type_klass(shortArrayKlassObj(), CHECK);
    initialize_basic_type_klass(intArrayKlassObj(), CHECK);
    initialize_basic_type_klass(longArrayKlassObj(), CHECK);
  } // end of core bootstrapping

  // Maybe this could be lifted up now that object array can be initialized
  // during the bootstrapping.

  // OLD
  // Initialize _objectArrayKlass after core bootstraping to make
  // sure the super class is set up properly for _objectArrayKlass.
  // ---
  // NEW
  // Since some of the old system object arrays have been converted to
  // ordinary object arrays, _objectArrayKlass will be loaded when
  // SystemDictionary::initialize(CHECK); is run. See the extra check
  // for Object_klass_loaded in objArrayKlassKlass::allocate_objArray_klass_impl.
  _objectArrayKlassObj = InstanceKlass::
    cast(SystemDictionary::Object_klass())->array_klass(1, CHECK);
  // OLD
  // Add the class to the class hierarchy manually to make sure that
  // its vtable is initialized after core bootstrapping is completed.
  // ---
  // New
  // Have already been initialized.
  _objectArrayKlassObj->append_to_sibling_list();

  // Compute is_jdk version flags.
  // Only 1.3 or later has the java.lang.Shutdown class.
  // Only 1.4 or later has the java.lang.CharSequence interface.
  // Only 1.5 or later has the java.lang.management.MemoryUsage class.
  if (JDK_Version::is_partially_initialized()) {
    uint8_t jdk_version;
    Klass* k = SystemDictionary::resolve_or_null(
        vmSymbols::java_lang_management_MemoryUsage(), THREAD);
    CLEAR_PENDING_EXCEPTION; // ignore exceptions
    if (k == NULL) {
      k = SystemDictionary::resolve_or_null(
          vmSymbols::java_lang_CharSequence(), THREAD);
      CLEAR_PENDING_EXCEPTION; // ignore exceptions
      if (k == NULL) {
        k = SystemDictionary::resolve_or_null(
            vmSymbols::java_lang_Shutdown(), THREAD);
        CLEAR_PENDING_EXCEPTION; // ignore exceptions
        if (k == NULL) {
          jdk_version = 2;
        } else {
          jdk_version = 3;
        }
      } else {
        jdk_version = 4;
      }
    } else {
      jdk_version = 5;
    }
    JDK_Version::fully_initialize(jdk_version);
  }

  #ifdef ASSERT
  if (FullGCALot) {
    // Allocate an array of dummy objects.
    // We'd like these to be at the bottom of the old generation,
    // so that when we free one and then collect,
    // (almost) the whole heap moves
    // and we find out if we actually update all the oops correctly.
    // But we can't allocate directly in the old generation,
    // so we allocate wherever, and hope that the first collection
    // moves these objects to the bottom of the old generation.
    // We can allocate directly in the permanent generation, so we do.
    int size;
    if (UseConcMarkSweepGC) {
      warning("Using +FullGCALot with concurrent mark sweep gc "
              "will not force all objects to relocate");
      size = FullGCALotDummies;
    } else {
      size = FullGCALotDummies * 2;
    }
    objArrayOop    naked_array = oopFactory::new_objArray(SystemDictionary::Object_klass(), size, CHECK);
    objArrayHandle dummy_array(THREAD, naked_array);
    int i = 0;
    while (i < size) {
        // Allocate dummy in old generation
      oop dummy = InstanceKlass::cast(SystemDictionary::Object_klass())->allocate_instance(CHECK);
      dummy_array->obj_at_put(i++, dummy);
    }
    {
      // Only modify the global variable inside the mutex.
      // If we had a race to here, the other dummy_array instances
      // and their elements just get dropped on the floor, which is fine.
      MutexLocker ml(FullGCALot_lock);
      if (_fullgc_alot_dummy_array == NULL) {
        _fullgc_alot_dummy_array = dummy_array();
      }
    }
    assert(i == _fullgc_alot_dummy_array->length(), "just checking");
  }
  #endif

  // Initialize dependency array for null class loader
  ClassLoaderData::the_null_class_loader_data()->init_dependencies(CHECK);

}

// CDS support for patching vtables in metadata in the shared archive.
// All types inherited from Metadata have vtables, but not types inherited
// from MetaspaceObj, because the latter does not have virtual functions.
// If the metadata type has a vtable, it cannot be shared in the read-only
// section of the CDS archive, because the vtable pointer is patched.
static inline void add_vtable(void** list, int* n, void* o, int count) {
  guarantee((*n) < count, "vtable list too small");
  void* vtable = dereference_vptr(o);
  assert(*(void**)(vtable) != NULL, "invalid vtable");
  list[(*n)++] = vtable;
}

void Universe::init_self_patching_vtbl_list(void** list, int count) {
  D_WARN_Unimplement;
}

void Universe::initialize_basic_type_mirrors(TRAPS) {
    assert(_int_mirror==NULL, "basic type mirrors already initialized");
    _int_mirror     =
      java_lang_Class::create_basic_type_mirror("int",    T_INT, CHECK);
    _float_mirror   =
      java_lang_Class::create_basic_type_mirror("float",  T_FLOAT,   CHECK);
    _double_mirror  =
      java_lang_Class::create_basic_type_mirror("double", T_DOUBLE,  CHECK);
    _byte_mirror    =
      java_lang_Class::create_basic_type_mirror("byte",   T_BYTE, CHECK);
    _bool_mirror    =
      java_lang_Class::create_basic_type_mirror("boolean",T_BOOLEAN, CHECK);
    _char_mirror    =
      java_lang_Class::create_basic_type_mirror("char",   T_CHAR, CHECK);
    _long_mirror    =
      java_lang_Class::create_basic_type_mirror("long",   T_LONG, CHECK);
    _short_mirror   =
      java_lang_Class::create_basic_type_mirror("short",  T_SHORT,   CHECK);
    _void_mirror    =
      java_lang_Class::create_basic_type_mirror("void",   T_VOID, CHECK);

    _mirrors[T_INT]     = _int_mirror;
    _mirrors[T_FLOAT]   = _float_mirror;
    _mirrors[T_DOUBLE]  = _double_mirror;
    _mirrors[T_BYTE]    = _byte_mirror;
    _mirrors[T_BOOLEAN] = _bool_mirror;
    _mirrors[T_CHAR]    = _char_mirror;
    _mirrors[T_LONG]    = _long_mirror;
    _mirrors[T_SHORT]   = _short_mirror;
    _mirrors[T_VOID]    = _void_mirror;
  //_mirrors[T_OBJECT]  = InstanceKlass::cast(_object_klass)->java_mirror();
  //_mirrors[T_ARRAY]   = InstanceKlass::cast(_object_klass)->java_mirror();
}

void Universe::fixup_mirrors(TRAPS) {
  // Bootstrap problem: all classes gets a mirror (java.lang.Class instance) assigned eagerly,
  // but we cannot do that for classes created before java.lang.Class is loaded. Here we simply
  // walk over permanent objects created so far (mostly classes) and fixup their mirrors. Note
  // that the number of objects allocated at this point is very small.
  assert(SystemDictionary::Class_klass_loaded(), "java.lang.Class should be loaded");
  HandleMark hm(THREAD);
  // Cache the start of the static fields
  InstanceMirrorKlass::init_offset_of_static_fields();

  GrowableArray <Klass*>* list = java_lang_Class::fixup_mirror_list();
  int list_length = list->length();
  for (int i = 0; i < list_length; i++) {
    Klass* k = list->at(i);
    assert(k->is_klass(), "List should only hold classes");
    EXCEPTION_MARK;
    KlassHandle kh(THREAD, k);
    java_lang_Class::fixup_mirror(kh, CATCH);
}
  delete java_lang_Class::fixup_mirror_list();
  java_lang_Class::set_fixup_mirror_list(NULL);
}

static bool has_run_finalizers_on_exit = false;

void Universe::run_finalizers_on_exit() {
  D_WARN_Unimplement;
}


// initialize_vtable could cause gc if
// 1) we specified true to initialize_vtable and
// 2) this ran after gc was enabled
// In case those ever change we use handles for oops
void Universe::reinitialize_vtable_of(KlassHandle k_h, TRAPS) {
  // init vtable of k and all subclasses
  Klass* ko = k_h();
  klassVtable* vt = ko->vtable();
  if (vt) vt->initialize_vtable(false, CHECK);
  if (ko->oop_is_instance()) {
    InstanceKlass* ik = (InstanceKlass*)ko;
    for (KlassHandle s_h(THREAD, ik->subklass());
         s_h() != NULL;
         s_h = KlassHandle(THREAD, s_h()->next_sibling())) {
      reinitialize_vtable_of(s_h, CHECK);
    }
  }
}


void initialize_itable_for_klass(Klass* k, TRAPS) {
  InstanceKlass::cast(k)->itable()->initialize_itable(false, CHECK);
}


void Universe::reinitialize_itables(TRAPS) {
  SystemDictionary::classes_do(initialize_itable_for_klass, CHECK);

}


bool Universe::on_page_boundary(void* addr) {
  return ((uintptr_t) addr) % os::vm_page_size() == 0;
}


bool Universe::should_fill_in_stack_trace(Handle throwable) {
  // never attempt to fill in the stack trace of preallocated errors that do not have
  // backtrace. These errors are kept alive forever and may be "re-used" when all
  // preallocated errors with backtrace have been consumed. Also need to avoid
  // a potential loop which could happen if an out of memory occurs when attempting
  // to allocate the backtrace.
  return ((throwable() != Universe::_out_of_memory_error_java_heap) &&
          (throwable() != Universe::_out_of_memory_error_metaspace)  &&
          (throwable() != Universe::_out_of_memory_error_class_metaspace)  &&
          (throwable() != Universe::_out_of_memory_error_array_size) &&
          (throwable() != Universe::_out_of_memory_error_gc_overhead_limit) &&
          (throwable() != Universe::_out_of_memory_error_realloc_objects));
}


oop Universe::gen_out_of_memory_error(oop default_err) {
  // generate an out of memory error:
  // - if there is a preallocated error with backtrace available then return it wth
  //   a filled in stack trace.
  // - if there are no preallocated errors with backtrace available then return
  //   an error without backtrace.
  int next;
  if (_preallocated_out_of_memory_error_avail_count > 0) {
    next = (int)Atomic::add(-1, &_preallocated_out_of_memory_error_avail_count);
    assert(next < (int)PreallocatedOutOfMemoryErrorCount, "avail count is corrupt");
  } else {
    next = -1;
  }
  if (next < 0) {
    // all preallocated errors have been used.
    // return default
    return default_err;
  } else {
    // get the error object at the slot and set set it to NULL so that the
    // array isn't keeping it alive anymore.
    oop exc = preallocated_out_of_memory_errors()->obj_at(next);
    assert(exc != NULL, "slot has been used already");
    preallocated_out_of_memory_errors()->obj_at_put(next, NULL);

    // use the message from the default error
    oop msg = java_lang_Throwable::message(default_err);
    assert(msg != NULL, "no message");
    java_lang_Throwable::set_message(exc, msg);

    // populate the stack trace and return it.
    java_lang_Throwable::fill_in_stack_trace_of_preallocated_backtrace(exc);
    return exc;
  }
}

intptr_t Universe::_non_oop_bits = 0;

void* Universe::non_oop_word() {
  // Neither the high bits nor the low bits of this value is allowed
  // to look like (respectively) the high or low bits of a real oop.
  //
  // High and low are CPU-specific notions, but low always includes
  // the low-order bit.  Since oops are always aligned at least mod 4,
  // setting the low-order bit will ensure that the low half of the
  // word will never look like that of a real oop.
  //
  // Using the OS-supplied non-memory-address word (usually 0 or -1)
  // will take care of the high bits, however many there are.

  if (_non_oop_bits == 0) {
    _non_oop_bits = (intptr_t)os::non_memory_address_word() | 1;
  }

  return (void*)_non_oop_bits;
}

jint universe_init() {
  D_WARN_Unimplement;
}

// Choose the heap base address and oop encoding mode
// when compressed oops are used:
// Unscaled  - Use 32-bits oops without encoding when
//     NarrowOopHeapBaseMin + heap_size < 4Gb
// ZeroBased - Use zero based compressed oops with encoding when
//     NarrowOopHeapBaseMin + heap_size < 32Gb
// HeapBased - Use compressed oops with heap base + encoding.

// 4Gb
static const uint64_t UnscaledOopHeapMax = (uint64_t(max_juint) + 1);
// 32Gb
// OopEncodingHeapMax == UnscaledOopHeapMax << LogMinObjAlignmentInBytes;

char* Universe::preferred_heap_base(size_t heap_size, size_t alignment, NARROW_OOP_MODE mode) {
  assert(is_size_aligned((size_t)OopEncodingHeapMax, alignment), "Must be");
  assert(is_size_aligned((size_t)UnscaledOopHeapMax, alignment), "Must be");
  assert(is_size_aligned(heap_size, alignment), "Must be");

  uintx heap_base_min_address_aligned = align_size_up(HeapBaseMinAddress, alignment);

  size_t base = 0;
#ifdef _LP64
  if (UseCompressedOops) {
    assert(mode == UnscaledNarrowOop  ||
           mode == ZeroBasedNarrowOop ||
           mode == HeapBasedNarrowOop, "mode is invalid");
    const size_t total_size = heap_size + heap_base_min_address_aligned;
    // Return specified base for the first request.
    if (!FLAG_IS_DEFAULT(HeapBaseMinAddress) && (mode == UnscaledNarrowOop)) {
      base = heap_base_min_address_aligned;

    // If the total size is small enough to allow UnscaledNarrowOop then
    // just use UnscaledNarrowOop.
    } else if ((total_size <= OopEncodingHeapMax) && (mode != HeapBasedNarrowOop)) {
      if ((total_size <= UnscaledOopHeapMax) && (mode == UnscaledNarrowOop) &&
          (Universe::narrow_oop_shift() == 0)) {
        // Use 32-bits oops without encoding and
        // place heap's top on the 4Gb boundary
        base = (UnscaledOopHeapMax - heap_size);
      } else {
        // Can't reserve with NarrowOopShift == 0
        Universe::set_narrow_oop_shift(LogMinObjAlignmentInBytes);

        if (mode == UnscaledNarrowOop ||
            mode == ZeroBasedNarrowOop && total_size <= UnscaledOopHeapMax) {

          // Use zero based compressed oops with encoding and
          // place heap's top on the 32Gb boundary in case
          // total_size > 4Gb or failed to reserve below 4Gb.
          uint64_t heap_top = OopEncodingHeapMax;

          // For small heaps, save some space for compressed class pointer
          // space so it can be decoded with no base.
          if (UseCompressedClassPointers && !UseSharedSpaces &&
              OopEncodingHeapMax <= 32*G) {

            uint64_t class_space = align_size_up(CompressedClassSpaceSize, alignment);
            assert(is_size_aligned((size_t)OopEncodingHeapMax-class_space,
                   alignment), "difference must be aligned too");
            uint64_t new_top = OopEncodingHeapMax-class_space;

            if (total_size <= new_top) {
              heap_top = new_top;
            }
          }

          // Align base to the adjusted top of the heap
          base = heap_top - heap_size;
        }
      }
    } else {
      // UnscaledNarrowOop encoding didn't work, and no base was found for ZeroBasedOops or
      // HeapBasedNarrowOop encoding was requested.  So, can't reserve below 32Gb.
      Universe::set_narrow_oop_shift(LogMinObjAlignmentInBytes);
    }

    // Set narrow_oop_base and narrow_oop_use_implicit_null_checks
    // used in ReservedHeapSpace() constructors.
    // The final values will be set in initialize_heap() below.
    if ((base != 0) && ((base + heap_size) <= OopEncodingHeapMax)) {
      // Use zero based compressed oops
      Universe::set_narrow_oop_base(NULL);
      // Don't need guard page for implicit checks in indexed
      // addressing mode with zero based Compressed Oops.
      Universe::set_narrow_oop_use_implicit_null_checks(true);
    } else {
      // Set to a non-NULL value so the ReservedSpace ctor computes
      // the correct no-access prefix.
      // The final value will be set in initialize_heap() below.
      Universe::set_narrow_oop_base((address)UnscaledOopHeapMax);
#if defined(_WIN64) || defined(AIX)
      if (UseLargePages) {
        // Cannot allocate guard pages for implicit checks in indexed
        // addressing mode when large pages are specified on windows.
        Universe::set_narrow_oop_use_implicit_null_checks(false);
      }
#endif //  _WIN64
    }
  }
#endif

  assert(is_ptr_aligned((char*)base, alignment), "Must be");
  return (char*)base; // also return NULL (don't care) for 32-bit VM
}

jint Universe::initialize_heap() {
  D_WARN_Unimplement;
}

void Universe::print_compressed_oops_mode() {
  D_WARN_Unimplement;
}

// Reserve the Java heap, which is now the same for all GCs.

// It's the caller's responsibility to ensure glitch-freedom
// (if required).
void Universe::update_heap_info_at_gc() {
  _heap_capacity_at_last_gc = 0;
  _heap_used_at_last_gc     = 0;
}


const char* Universe::narrow_oop_mode_to_string(Universe::NARROW_OOP_MODE mode) {
  switch (mode) {
    case UnscaledNarrowOop:
      return "32-bit";
    case ZeroBasedNarrowOop:
      return "Zero based";
    case HeapBasedNarrowOop:
      return "Non-zero based";
  }

  ShouldNotReachHere();
  return "";
}


Universe::NARROW_OOP_MODE Universe::narrow_oop_mode() {
  if (narrow_oop_base() != 0) {
    return HeapBasedNarrowOop;
  }

  if (narrow_oop_shift() != 0) {
    return ZeroBasedNarrowOop;
  }

  return UnscaledNarrowOop;
}


void universe2_init() {
  EXCEPTION_MARK;
  Universe::genesis(CATCH);
}


// This function is defined in JVM.cpp
extern void initialize_converter_functions();

bool universe_post_init() {
  assert(!is_init_completed(), "Error: initialization not yet completed!");
  Universe::_fully_initialized = true;
  EXCEPTION_MARK;
  { ResourceMark rm;
    Interpreter::initialize();      // needed for interpreter entry points
    if (!UseSharedSpaces) {
      HandleMark hm(THREAD);
      KlassHandle ok_h(THREAD, SystemDictionary::Object_klass());
      Universe::reinitialize_vtable_of(ok_h, CHECK_false);
      Universe::reinitialize_itables(CHECK_false);
    }
  }

  HandleMark hm(THREAD);
  Klass* k;
  instanceKlassHandle k_h;
  // Setup preallocated empty java.lang.Class array
  Universe::_the_empty_class_klass_array = oopFactory::new_objArray(SystemDictionary::Class_klass(), 0, CHECK_false);

  // Setup preallocated OutOfMemoryError errors
  k = SystemDictionary::resolve_or_fail(vmSymbols::java_lang_OutOfMemoryError(), true, CHECK_false);
  k_h = instanceKlassHandle(THREAD, k);
  Universe::_out_of_memory_error_java_heap = k_h->allocate_instance(CHECK_false);
  Universe::_out_of_memory_error_metaspace = k_h->allocate_instance(CHECK_false);
  Universe::_out_of_memory_error_class_metaspace = k_h->allocate_instance(CHECK_false);
  Universe::_out_of_memory_error_array_size = k_h->allocate_instance(CHECK_false);
  Universe::_out_of_memory_error_gc_overhead_limit =
          k_h->allocate_instance(CHECK_false);
  Universe::_out_of_memory_error_realloc_objects = k_h->allocate_instance(CHECK_false);

  // Setup preallocated NullPointerException
  // (this is currently used for a cheap & dirty solution in compiler exception handling)
  k = SystemDictionary::resolve_or_fail(vmSymbols::java_lang_NullPointerException(), true, CHECK_false);
  Universe::_null_ptr_exception_instance = InstanceKlass::cast(k)->allocate_instance(CHECK_false);
  // Setup preallocated ArithmeticException
  // (this is currently used for a cheap & dirty solution in compiler exception handling)
  k = SystemDictionary::resolve_or_fail(vmSymbols::java_lang_ArithmeticException(), true, CHECK_false);
  Universe::_arithmetic_exception_instance = InstanceKlass::cast(k)->allocate_instance(CHECK_false);
  // Virtual Machine Error for when we get into a situation we can't resolve
  k = SystemDictionary::resolve_or_fail(
          vmSymbols::java_lang_VirtualMachineError(), true, CHECK_false);
  bool linked = InstanceKlass::cast(k)->link_class_or_fail(CHECK_false);
  if (!linked) {
    tty->print_cr("Unable to link/verify VirtualMachineError class");
    return false; // initialization failed
  }
  Universe::_virtual_machine_error_instance =
          InstanceKlass::cast(k)->allocate_instance(CHECK_false);

  Universe::_vm_exception = InstanceKlass::cast(k)->allocate_instance(CHECK_false);

  if (!DumpSharedSpaces) {
    // These are the only Java fields that are currently set during shared space dumping.
    // We prefer to not handle this generally, so we always reinitialize these detail messages.
    Handle msg = java_lang_String::create_from_str("Java heap space", CHECK_false);
    java_lang_Throwable::set_message(Universe::_out_of_memory_error_java_heap, msg());

    msg = java_lang_String::create_from_str("Metaspace", CHECK_false);
    java_lang_Throwable::set_message(Universe::_out_of_memory_error_metaspace, msg());
    msg = java_lang_String::create_from_str("Compressed class space", CHECK_false);
    java_lang_Throwable::set_message(Universe::_out_of_memory_error_class_metaspace, msg());

    msg = java_lang_String::create_from_str("Requested array size exceeds VM limit", CHECK_false);
    java_lang_Throwable::set_message(Universe::_out_of_memory_error_array_size, msg());

    msg = java_lang_String::create_from_str("GC overhead limit exceeded", CHECK_false);
    java_lang_Throwable::set_message(Universe::_out_of_memory_error_gc_overhead_limit, msg());

    msg = java_lang_String::create_from_str("Java heap space: failed reallocation of scalar replaced objects", CHECK_false);
    java_lang_Throwable::set_message(Universe::_out_of_memory_error_realloc_objects, msg());

    msg = java_lang_String::create_from_str("/ by zero", CHECK_false);
    java_lang_Throwable::set_message(Universe::_arithmetic_exception_instance, msg());

    // Setup the array of errors that have preallocated backtrace
    k = Universe::_out_of_memory_error_java_heap->klass();
    assert(k->name() == vmSymbols::java_lang_OutOfMemoryError(), "should be out of memory error");
    k_h = instanceKlassHandle(THREAD, k);

    int len = (StackTraceInThrowable) ? (int)PreallocatedOutOfMemoryErrorCount : 0;
    Universe::_preallocated_out_of_memory_error_array = oopFactory::new_objArray(k_h(), len, CHECK_false);
    for (int i=0; i<len; i++) {
      oop err = k_h->allocate_instance(CHECK_false);
      Handle err_h = Handle(THREAD, err);
      java_lang_Throwable::allocate_backtrace(err_h, CHECK_false);
      Universe::preallocated_out_of_memory_errors()->obj_at_put(i, err_h());
    }
    Universe::_preallocated_out_of_memory_error_avail_count = (jint)len;
  }


  // Setup static method for registering finalizers
  // The finalizer klass must be linked before looking up the method, in
  // case it needs to get rewritten.
  // TODO
//  InstanceKlass::cast(SystemDictionary::Finalizer_klass())->link_class(CHECK_false);
//  Method* m = InstanceKlass::cast(SystemDictionary::Finalizer_klass())->find_method(
//          vmSymbols::register_method_name(),
//          vmSymbols::register_method_signature());
//  if (m == NULL || !m->is_static()) {
//    tty->print_cr("Unable to link/verify Finalizer.register method");
//    return false; // initialization failed (cannot throw exception yet)
//  }
//  Universe::_finalizer_register_cache->init(
//          SystemDictionary::Finalizer_klass(), m);
  Method *m;
  InstanceKlass::cast(SystemDictionary::misc_Unsafe_klass())->link_class(CHECK_false);
  m = InstanceKlass::cast(SystemDictionary::misc_Unsafe_klass())->find_method(
          vmSymbols::throwIllegalAccessError_name(),
          vmSymbols::void_method_signature());
  if (m != NULL && !m->is_static()) {
    // Note null is okay; this method is used in itables, and if it is null,
    // then AbstractMethodError is thrown instead.
    tty->print_cr("Unable to link/verify Unsafe.throwIllegalAccessError method");
    return false; // initialization failed (cannot throw exception yet)
  }
  // TODO:
//  Universe::_throw_illegal_access_error_cache->init(
//          SystemDictionary::misc_Unsafe_klass(), m);

  // Setup method for registering loaded classes in class loader vector
  InstanceKlass::cast(SystemDictionary::ClassLoader_klass())->link_class(CHECK_false);
  m = InstanceKlass::cast(SystemDictionary::ClassLoader_klass())->find_method(vmSymbols::addClass_name(), vmSymbols::class_void_signature());
  if (m == NULL || m->is_static()) {
    tty->print_cr("Unable to link/verify ClassLoader.addClass method");
    return false; // initialization failed (cannot throw exception yet)
  }
//  Universe::_loader_addClass_cache->init(
//          SystemDictionary::ClassLoader_klass(), m);

  // Setup method for checking protection domain
  InstanceKlass::cast(SystemDictionary::ProtectionDomain_klass())->link_class(CHECK_false);
  m = InstanceKlass::cast(SystemDictionary::ProtectionDomain_klass())->
          find_method(vmSymbols::impliesCreateAccessControlContext_name(),
                      vmSymbols::void_boolean_signature());
  // Allow NULL which should only happen with bootstrapping.
  if (m != NULL) {
    if (m->is_static()) {
      // NoSuchMethodException doesn't actually work because it tries to run the
      // <init> function before java_lang_Class is linked. Print error and exit.
      tty->print_cr("ProtectionDomain.impliesCreateAccessControlContext() has the wrong linkage");
      return false; // initialization failed
    }
//    Universe::_pd_implies_cache->init(
//            SystemDictionary::ProtectionDomain_klass(), m);
  }

  // The folowing is initializing converter functions for serialization in
  // JVM.cpp. If we clean up the StrictMath code above we may want to find
  // a better solution for this as well.
  initialize_converter_functions();

  // This needs to be done before the first scavenge/gc, since
  // it's an input to soft ref clearing policy.
  {
    MutexLocker x(Heap_lock);
    Universe::update_heap_info_at_gc();
  }

  return true;
}


void Universe::compute_base_vtable_size() {
  _base_vtable_size = ClassLoader::compute_Object_vtable();
}


// %%% The Universe::flush_foo methods belong in CodeCache.

// Flushes compiled methods dependent on dependee.
void Universe::flush_dependents_on(instanceKlassHandle dependee) {
  D_WARN_Unimplement;
}

// Flushes compiled methods dependent on a particular CallSite
// instance when its target is different than the given MethodHandle.
void Universe::flush_dependents_on(Handle call_site, Handle method_handle) {
  D_WARN_Unimplement;
}

#ifdef HOTSWAP
// Flushes compiled methods dependent on dependee in the evolutionary sense
void Universe::flush_evol_dependents_on(instanceKlassHandle ev_k_h) {
  D_WARN_Unimplement;
}
#endif // HOTSWAP


// Flushes compiled methods dependent on dependee
void Universe::flush_dependents_on_method(methodHandle m_h) {
  D_WARN_Unimplement;
}

void Universe::print() {
  print_on(gclog_or_tty);
}

void Universe::print_on(outputStream* st, bool extended) {
  D_WARN_Unimplement;
}

void Universe::print_heap_at_SIGBREAK() {
  D_WARN_Unimplement;
}

void Universe::print_heap_before_gc(outputStream* st, bool ignore_extended) {
  D_WARN_Unimplement;
}

void Universe::print_heap_after_gc(outputStream* st, bool ignore_extended) {
  D_WARN_Unimplement;
}

void Universe::initialize_verify_flags() {
  D_WARN_Unimplement;
}

bool Universe::should_verify_subset(uint subset) {
  if (verify_flags & subset) {
    return true;
  }
  return false;
}

void Universe::verify(VerifyOption option, const char* prefix, bool silent) {
  D_WARN_Unimplement;
}

// Oop verification (see MacroAssembler::verify_oop)

static uintptr_t _verify_oop_data[2]   = {0, (uintptr_t)-1};
static uintptr_t _verify_klass_data[2] = {0, (uintptr_t)-1};


#ifndef PRODUCT

static void calculate_verify_data(uintptr_t verify_data[2],
                                  HeapWord* low_boundary,
                                  HeapWord* high_boundary) {
  assert(low_boundary < high_boundary, "bad interval");

  // decide which low-order bits we require to be clear:
  size_t alignSize = MinObjAlignmentInBytes;
  size_t min_object_size = CollectedHeap::min_fill_size();

  // make an inclusive limit:
  uintptr_t max = (uintptr_t)high_boundary - min_object_size*wordSize;
  uintptr_t min = (uintptr_t)low_boundary;
  assert(min < max, "bad interval");
  uintptr_t diff = max ^ min;

  // throw away enough low-order bits to make the diff vanish
  uintptr_t mask = (uintptr_t)(-1);
  while ((mask & diff) != 0)
    mask <<= 1;
  uintptr_t bits = (min & mask);
  assert(bits == (max & mask), "correct mask");
  // check an intermediate value between min and max, just to make sure:
  assert(bits == ((min + (max-min)/2) & mask), "correct mask");

  // require address alignment, too:
  mask |= (alignSize - 1);

  if (!(verify_data[0] == 0 && verify_data[1] == (uintptr_t)-1)) {
    assert(verify_data[0] == mask && verify_data[1] == bits, "mask stability");
  }
  verify_data[0] = mask;
  verify_data[1] = bits;
}

// Oop verification (see MacroAssembler::verify_oop)

uintptr_t Universe::verify_oop_mask() {
  MemRegion m = heap()->reserved_region();
  calculate_verify_data(_verify_oop_data,
                        m.start(),
                        m.end());
  return _verify_oop_data[0];
}



uintptr_t Universe::verify_oop_bits() {
  verify_oop_mask();
  return _verify_oop_data[1];
}

uintptr_t Universe::verify_mark_mask() {
  return markOopDesc::lock_mask_in_place;
}

uintptr_t Universe::verify_mark_bits() {
  intptr_t mask = verify_mark_mask();
  intptr_t bits = (intptr_t)markOopDesc::prototype();
  assert((bits & ~mask) == 0, "no stray header bits");
  return bits;
}
#endif // PRODUCT


void Universe::compute_verify_oop_data() {
  verify_oop_mask();
  verify_oop_bits();
  verify_mark_mask();
  verify_mark_bits();
}


void LatestMethodCache::init(Klass* k, Method* m) {
  if (!UseSharedSpaces) {
    _klass = k;
  }
#ifndef PRODUCT
  else {
    // sharing initilization should have already set up _klass
    assert(_klass != NULL, "just checking");
  }
#endif

  _method_idnum = m->method_idnum();
  assert(_method_idnum >= 0, "sanity check");
}


Method* LatestMethodCache::get_method() {
  if (klass() == NULL) return NULL;
  InstanceKlass* ik = InstanceKlass::cast(klass());
  Method* m = ik->method_with_idnum(method_idnum());
  assert(m != NULL, "sanity check");
  return m;
}


#ifdef ASSERT
// Release dummy object(s) at bottom of heap
bool Universe::release_fullgc_alot_dummy() {
  MutexLocker ml(FullGCALot_lock);
  if (_fullgc_alot_dummy_array != NULL) {
    if (_fullgc_alot_dummy_next >= _fullgc_alot_dummy_array->length()) {
      // No more dummies to release, release entire array instead
      _fullgc_alot_dummy_array = NULL;
      return false;
    }
    if (!UseConcMarkSweepGC) {
      // Release dummy at bottom of old generation
      _fullgc_alot_dummy_array->obj_at_put(_fullgc_alot_dummy_next++, NULL);
    }
    // Release dummy at bottom of permanent generation
    _fullgc_alot_dummy_array->obj_at_put(_fullgc_alot_dummy_next++, NULL);
  }
  return true;
}

#endif // ASSERT
