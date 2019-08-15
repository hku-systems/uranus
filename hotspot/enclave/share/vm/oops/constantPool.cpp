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
#include "classfile/classLoaderData.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "oops/constantPool.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/objArrayKlass.hpp"
#include "runtime/signature.hpp"

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

ConstantPool* ConstantPool::allocate(ClassLoaderData* loader_data, int length, TRAPS) {
  D_WARN_Unimplement;
}

ConstantPool::ConstantPool(Array<u1>* tags) {
  D_WARN_Unimplement;
}

void ConstantPool::deallocate_contents(ClassLoaderData* loader_data) {
  D_WARN_Unimplement;
}

void ConstantPool::release_C_heap_structures() {
  // walk constant pool and decrement symbol reference counts
  unreference_symbols();

  delete _lock;
  set_lock(NULL);
}

objArrayOop ConstantPool::resolved_references() const {
  return (objArrayOop)JNIHandles::resolve(_resolved_references);
}

// Create resolved_references array and mapping array for original cp indexes
// The ldc bytecode was rewritten to have the resolved reference array index so need a way
// to map it back for resolving and some unlikely miscellaneous uses.
// The objects created by invokedynamic are appended to this list.
void ConstantPool::initialize_resolved_references(ClassLoaderData* loader_data,
                                                  intStack reference_map,
                                                  int constant_pool_map_length,
                                                  TRAPS) {
  // Initialized the resolved object cache.
  D_WARN_Unimplement;
}

// CDS support. Create a new resolved_references array.
void ConstantPool::restore_unshareable_info(TRAPS) {
  D_WARN_Unimplement;
}

void ConstantPool::remove_unshareable_info() {
  // Resolved references are not in the shared archive.
  // Save the length for restoration.  It is not necessarily the same length
  // as reference_map.length() if invokedynamic is saved.
  set_resolved_reference_length(
    resolved_references() != NULL ? resolved_references()->length() : 0);
  set_resolved_references(NULL);
  set_lock(NULL);
}

int ConstantPool::cp_to_object_index(int cp_index) {
  // this is harder don't do this so much.
  int i = reference_map()->find(cp_index);
  // We might not find the index for jsr292 call.
  return (i < 0) ? _no_index_sentinel : i;
}

Klass* ConstantPool::klass_at_impl(constantPoolHandle this_oop, int which, TRAPS) {
  return (Klass*)JVM_ENTRY_resolve_klass(JavaThread::current(), this_oop(), which);
}


// Does not update ConstantPool* - to avoid any exception throwing. Used
// by compiler and exception handling.  Also used to avoid classloads for
// instanceof operations. Returns NULL if the class has not been loaded or
// if the verification of constant pool failed
Klass* ConstantPool::klass_at_if_loaded(constantPoolHandle this_oop, int which) {
  CPSlot entry = this_oop->slot_at(which);
  if (entry.is_resolved()) {
    assert(entry.get_klass()->is_klass(), "must be");
    return entry.get_klass();
  }
  return NULL;
}


Klass* ConstantPool::klass_ref_at_if_loaded(constantPoolHandle this_oop, int which) {
  return klass_at_if_loaded(this_oop, this_oop->klass_ref_index_at(which));
}


Method* ConstantPool::method_at_if_loaded(constantPoolHandle cpool,
                                                   int which) {
  if (cpool->cache() == NULL)  return NULL;  // nothing to load yet
  int cache_index = decode_cpcache_index(which, true);
  if (!(cache_index >= 0 && cache_index < cpool->cache()->length())) {
    // FIXME: should be an assert
    if (PrintMiscellaneous && (Verbose||WizardMode)) {
      tty->print_cr("bad operand %d in:", which); cpool->print();
    }
    return NULL;
  }
  ConstantPoolCacheEntry* e = cpool->cache()->entry_at(cache_index);
  return e->method_if_resolved(cpool);
}


bool ConstantPool::has_appendix_at_if_loaded(constantPoolHandle cpool, int which) {
  if (cpool->cache() == NULL)  return false;  // nothing to load yet
  int cache_index = decode_cpcache_index(which, true);
  ConstantPoolCacheEntry* e = cpool->cache()->entry_at(cache_index);
  return e->has_appendix();
}

oop ConstantPool::appendix_at_if_loaded(constantPoolHandle cpool, int which) {
  if (cpool->cache() == NULL)  return NULL;  // nothing to load yet
  int cache_index = decode_cpcache_index(which, true);
  ConstantPoolCacheEntry* e = cpool->cache()->entry_at(cache_index);
  return e->appendix_if_resolved(cpool);
}


bool ConstantPool::has_method_type_at_if_loaded(constantPoolHandle cpool, int which) {
  if (cpool->cache() == NULL)  return false;  // nothing to load yet
  int cache_index = decode_cpcache_index(which, true);
  ConstantPoolCacheEntry* e = cpool->cache()->entry_at(cache_index);
  return e->has_method_type();
}

oop ConstantPool::method_type_at_if_loaded(constantPoolHandle cpool, int which) {
  if (cpool->cache() == NULL)  return NULL;  // nothing to load yet
  int cache_index = decode_cpcache_index(which, true);
  ConstantPoolCacheEntry* e = cpool->cache()->entry_at(cache_index);
  return e->method_type_if_resolved(cpool);
}


Symbol* ConstantPool::impl_name_ref_at(int which, bool uncached) {
  int name_index = name_ref_index_at(impl_name_and_type_ref_index_at(which, uncached));
  return symbol_at(name_index);
}


Symbol* ConstantPool::impl_signature_ref_at(int which, bool uncached) {
  int signature_index = signature_ref_index_at(impl_name_and_type_ref_index_at(which, uncached));
  return symbol_at(signature_index);
}


int ConstantPool::impl_name_and_type_ref_index_at(int which, bool uncached) {
  int i = which;
  if (!uncached && cache() != NULL) {
    if (ConstantPool::is_invokedynamic_index(which)) {
      // Invokedynamic index is index into resolved_references
      int pool_index = invokedynamic_cp_cache_entry_at(which)->constant_pool_index();
      pool_index = invoke_dynamic_name_and_type_ref_index_at(pool_index);
      assert(tag_at(pool_index).is_name_and_type(), "");
      return pool_index;
    }
    // change byte-ordering and go via cache
    i = remap_instruction_operand_from_cache(which);
  } else {
    if (tag_at(which).is_invoke_dynamic()) {
      int pool_index = invoke_dynamic_name_and_type_ref_index_at(which);
      assert(tag_at(pool_index).is_name_and_type(), "");
      return pool_index;
    }
  }
  assert(tag_at(i).is_field_or_method(), "Corrupted constant pool");
  assert(!tag_at(i).is_invoke_dynamic(), "Must be handled above");
  jint ref_index = *int_at_addr(i);
  return extract_high_short_from_int(ref_index);
}


int ConstantPool::impl_klass_ref_index_at(int which, bool uncached) {
  guarantee(!ConstantPool::is_invokedynamic_index(which),
            "an invokedynamic instruction does not have a klass");
  int i = which;
  if (!uncached && cache() != NULL) {
    // change byte-ordering and go via cache
    i = remap_instruction_operand_from_cache(which);
  }
  assert(tag_at(i).is_field_or_method(), "Corrupted constant pool");
  jint ref_index = *int_at_addr(i);
  return extract_low_short_from_int(ref_index);
}



int ConstantPool::remap_instruction_operand_from_cache(int operand) {
  int cpc_index = operand;
  DEBUG_ONLY(cpc_index -= CPCACHE_INDEX_TAG);
  assert((int)(u2)cpc_index == cpc_index, "clean u2");
  int member_index = cache()->entry_at(cpc_index)->constant_pool_index();
  return member_index;
}


void ConstantPool::verify_constant_pool_resolve(constantPoolHandle this_oop, KlassHandle k, TRAPS) {
  D_WARN_Unimplement;
}


int ConstantPool::name_ref_index_at(int which_nt) {
  jint ref_index = name_and_type_at(which_nt);
  return extract_low_short_from_int(ref_index);
}


int ConstantPool::signature_ref_index_at(int which_nt) {
  jint ref_index = name_and_type_at(which_nt);
  return extract_high_short_from_int(ref_index);
}


Klass* ConstantPool::klass_ref_at(int which, TRAPS) {
  return klass_at(klass_ref_index_at(which), CHECK_NULL);
}


Symbol* ConstantPool::klass_name_at(int which) {
  assert(tag_at(which).is_unresolved_klass() || tag_at(which).is_klass(),
         "Corrupted constant pool");
  // A resolved constantPool entry will contain a Klass*, otherwise a Symbol*.
  // It is not safe to rely on the tag bit's here, since we don't have a lock, and the entry and
  // tag is not updated atomicly.
  CPSlot entry = slot_at(which);
  if (entry.is_resolved()) {
    // Already resolved - return entry's name.
    assert(entry.get_klass()->is_klass(), "must be");
    return entry.get_klass()->name();
  } else {
    assert(entry.is_unresolved(), "must be either symbol or klass");
    return entry.get_symbol();
  }
}

Symbol* ConstantPool::klass_ref_at_noresolve(int which) {
  jint ref_index = klass_ref_index_at(which);
  return klass_at_noresolve(ref_index);
}

Symbol* ConstantPool::uncached_klass_ref_at_noresolve(int which) {
  jint ref_index = uncached_klass_ref_index_at(which);
  return klass_at_noresolve(ref_index);
}

char* ConstantPool::string_at_noresolve(int which) {
  Symbol* s = unresolved_string_at(which);
  if (s == NULL) {
    return (char*)"<pseudo-string>";
  } else {
    return unresolved_string_at(which)->as_C_string();
  }
}

BasicType ConstantPool::basic_type_for_signature_at(int which) {
  D_WARN_Unimplement;
}


void ConstantPool::resolve_string_constants_impl(constantPoolHandle this_oop, TRAPS) {
  for (int index = 1; index < this_oop->length(); index++) { // Index 0 is unused
    if (this_oop->tag_at(index).is_string()) {
      this_oop->string_at(index, CHECK);
    }
  }
}

// Resolve all the classes in the constant pool.  If they are all resolved,
// the constant pool is read-only.  Enhancement: allocate cp entries to
// another metaspace, and copy to read-only or read-write space if this
// bit is set.
bool ConstantPool::resolve_class_constants(TRAPS) {
  constantPoolHandle cp(THREAD, this);
  for (int index = 1; index < length(); index++) { // Index 0 is unused
    if (tag_at(index).is_unresolved_klass() &&
        klass_at_if_loaded(cp, index) == NULL) {
      return false;
  }
  }
  // set_preresolution(); or some bit for future use
  return true;
}

// If resolution for MethodHandle or MethodType fails, save the exception
// in the resolution error table, so that the same exception is thrown again.
void ConstantPool::save_and_throw_exception(constantPoolHandle this_oop, int which,
                                     int tag, TRAPS) {
  D_WARN_Unimplement;
}


// Called to resolve constants in the constant pool and return an oop.
// Some constant pool entries cache their resolved oop. This is also
// called to create oops from constants to use in arguments for invokedynamic
oop ConstantPool::resolve_constant_at_impl(constantPoolHandle this_oop, int index, int cache_index, TRAPS) {
    Bytecodes::Code code = Bytecodes::_ldc;
    bool resolve_later = false;
    if (index < 0) {
      code = Bytecodes::_fast_aldc;
      if (cache_index < 0) {
        // do not resolve it at this time
        cache_index = -cache_index;
        resolve_later = true;
      }
      oop result_oop = this_oop->resolved_references()->obj_at(cache_index);
      if (result_oop != NULL || resolve_later) return result_oop;
      index = cache_index;
    }
    return (oop)JVM_ENTRY_resolve_ldc(JavaThread::current(), this_oop(), index, code);
}

oop ConstantPool::uncached_string_at(int which, TRAPS) {
  D_WARN_Unimplement;
}


oop ConstantPool::resolve_bootstrap_specifier_at_impl(constantPoolHandle this_oop, int index, TRAPS) {
  D_WARN_Unimplement;
}

oop ConstantPool::string_at_impl(constantPoolHandle this_oop, int which, int obj_index, TRAPS) {
  D_WARN_Unimplement;
}


bool ConstantPool::klass_name_at_matches(instanceKlassHandle k,
                                                int which) {
  // Names are interned, so we can compare Symbol*s directly
  Symbol* cp_name = klass_name_at(which);
  return (cp_name == k->name());
}


// Iterate over symbols and decrement ones which are Symbol*s.
// This is done during GC so do not need to lock constantPool unless we
// have per-thread safepoints.
// Only decrement the UTF8 symbols. Unresolved classes and strings point to
// these symbols but didn't increment the reference count.
void ConstantPool::unreference_symbols() {
  for (int index = 1; index < length(); index++) { // Index 0 is unused
    constantTag tag = tag_at(index);
    if (tag.is_symbol()) {
      symbol_at(index)->decrement_refcount();
    }
  }
}


// Compare this constant pool's entry at index1 to the constant pool
// cp2's entry at index2.
bool ConstantPool::compare_entry_to(int index1, constantPoolHandle cp2,
       int index2, TRAPS) {

  // The error tags are equivalent to non-error tags when comparing
  jbyte t1 = tag_at(index1).non_error_value();
  jbyte t2 = cp2->tag_at(index2).non_error_value();

  if (t1 != t2) {
    // Not the same entry type so there is nothing else to check. Note
    // that this style of checking will consider resolved/unresolved
    // class pairs as different.
    // From the ConstantPool* API point of view, this is correct
    // behavior. See VM_RedefineClasses::merge_constant_pools() to see how this
    // plays out in the context of ConstantPool* merging.
    return false;
  }

  switch (t1) {
  case JVM_CONSTANT_Class:
  {
    Klass* k1 = klass_at(index1, CHECK_false);
    Klass* k2 = cp2->klass_at(index2, CHECK_false);
    if (k1 == k2) {
      return true;
    }
  } break;

  case JVM_CONSTANT_ClassIndex:
  {
    int recur1 = klass_index_at(index1);
    int recur2 = cp2->klass_index_at(index2);
    bool match = compare_entry_to(recur1, cp2, recur2, CHECK_false);
    if (match) {
      return true;
    }
  } break;

  case JVM_CONSTANT_Double:
  {
    jdouble d1 = double_at(index1);
    jdouble d2 = cp2->double_at(index2);
    if (d1 == d2) {
      return true;
    }
  } break;

  case JVM_CONSTANT_Fieldref:
  case JVM_CONSTANT_InterfaceMethodref:
  case JVM_CONSTANT_Methodref:
  {
    int recur1 = uncached_klass_ref_index_at(index1);
    int recur2 = cp2->uncached_klass_ref_index_at(index2);
    bool match = compare_entry_to(recur1, cp2, recur2, CHECK_false);
    if (match) {
      recur1 = uncached_name_and_type_ref_index_at(index1);
      recur2 = cp2->uncached_name_and_type_ref_index_at(index2);
      match = compare_entry_to(recur1, cp2, recur2, CHECK_false);
      if (match) {
        return true;
      }
    }
  } break;

  case JVM_CONSTANT_Float:
  {
    jfloat f1 = float_at(index1);
    jfloat f2 = cp2->float_at(index2);
    if (f1 == f2) {
      return true;
    }
  } break;

  case JVM_CONSTANT_Integer:
  {
    jint i1 = int_at(index1);
    jint i2 = cp2->int_at(index2);
    if (i1 == i2) {
      return true;
    }
  } break;

  case JVM_CONSTANT_Long:
  {
    jlong l1 = long_at(index1);
    jlong l2 = cp2->long_at(index2);
    if (l1 == l2) {
      return true;
    }
  } break;

  case JVM_CONSTANT_NameAndType:
  {
    int recur1 = name_ref_index_at(index1);
    int recur2 = cp2->name_ref_index_at(index2);
    bool match = compare_entry_to(recur1, cp2, recur2, CHECK_false);
    if (match) {
      recur1 = signature_ref_index_at(index1);
      recur2 = cp2->signature_ref_index_at(index2);
      match = compare_entry_to(recur1, cp2, recur2, CHECK_false);
      if (match) {
        return true;
      }
    }
  } break;

  case JVM_CONSTANT_StringIndex:
  {
    int recur1 = string_index_at(index1);
    int recur2 = cp2->string_index_at(index2);
    bool match = compare_entry_to(recur1, cp2, recur2, CHECK_false);
    if (match) {
      return true;
    }
  } break;

  case JVM_CONSTANT_UnresolvedClass:
  {
    Symbol* k1 = unresolved_klass_at(index1);
    Symbol* k2 = cp2->unresolved_klass_at(index2);
    if (k1 == k2) {
      return true;
    }
  } break;

  case JVM_CONSTANT_MethodType:
  {
    int k1 = method_type_index_at_error_ok(index1);
    int k2 = cp2->method_type_index_at_error_ok(index2);
    bool match = compare_entry_to(k1, cp2, k2, CHECK_false);
    if (match) {
      return true;
    }
  } break;

  case JVM_CONSTANT_MethodHandle:
  {
    int k1 = method_handle_ref_kind_at_error_ok(index1);
    int k2 = cp2->method_handle_ref_kind_at_error_ok(index2);
    if (k1 == k2) {
      int i1 = method_handle_index_at_error_ok(index1);
      int i2 = cp2->method_handle_index_at_error_ok(index2);
      bool match = compare_entry_to(i1, cp2, i2, CHECK_false);
      if (match) {
        return true;
      }
    }
  } break;

  case JVM_CONSTANT_InvokeDynamic:
  {
    int k1 = invoke_dynamic_name_and_type_ref_index_at(index1);
    int k2 = cp2->invoke_dynamic_name_and_type_ref_index_at(index2);
    int i1 = invoke_dynamic_bootstrap_specifier_index(index1);
    int i2 = cp2->invoke_dynamic_bootstrap_specifier_index(index2);
    // separate statements and variables because CHECK_false is used
    bool match_entry = compare_entry_to(k1, cp2, k2, CHECK_false);
    bool match_operand = compare_operand_to(i1, cp2, i2, CHECK_false);
    return (match_entry && match_operand);
  } break;

  case JVM_CONSTANT_String:
  {
    Symbol* s1 = unresolved_string_at(index1);
    Symbol* s2 = cp2->unresolved_string_at(index2);
    if (s1 == s2) {
      return true;
    }
  } break;

  case JVM_CONSTANT_Utf8:
  {
    Symbol* s1 = symbol_at(index1);
    Symbol* s2 = cp2->symbol_at(index2);
    if (s1 == s2) {
      return true;
    }
  } break;

  // Invalid is used as the tag for the second constant pool entry
  // occupied by JVM_CONSTANT_Double or JVM_CONSTANT_Long. It should
  // not be seen by itself.
  case JVM_CONSTANT_Invalid: // fall through

  default:
    ShouldNotReachHere();
    break;
  }

  return false;
} // end compare_entry_to()


// Resize the operands array with delta_len and delta_size.
// Used in RedefineClasses for CP merge.
void ConstantPool::resize_operands(int delta_len, int delta_size, TRAPS) {
  D_WARN_Unimplement;
} // end resize_operands()


// Extend the operands array with the length and size of the ext_cp operands.
// Used in RedefineClasses for CP merge.
void ConstantPool::extend_operands(constantPoolHandle ext_cp, TRAPS) {
  D_WARN_Unimplement;
} // end extend_operands()


// Shrink the operands array to a smaller array with new_len length.
// Used in RedefineClasses for CP merge.
void ConstantPool::shrink_operands(int new_len, TRAPS) {
  int old_len = operand_array_length(operands());
  if (new_len == old_len) {
    return; // nothing to do
  }
  assert(new_len < old_len, "shrunken operands array must be smaller");

  int free_base  = operand_next_offset_at(new_len - 1);
  int delta_len  = new_len - old_len;
  int delta_size = 2*delta_len + free_base - operands()->length();

  resize_operands(delta_len, delta_size, CHECK);

} // end shrink_operands()


void ConstantPool::copy_operands(constantPoolHandle from_cp,
                                 constantPoolHandle to_cp,
                                 TRAPS) {
  D_WARN_Unimplement;
} // end copy_operands()


// Copy this constant pool's entries at start_i to end_i (inclusive)
// to the constant pool to_cp's entries starting at to_i. A total of
// (end_i - start_i) + 1 entries are copied.
void ConstantPool::copy_cp_to_impl(constantPoolHandle from_cp, int start_i, int end_i,
       constantPoolHandle to_cp, int to_i, TRAPS) {


  int dest_i = to_i;  // leave original alone for debug purposes

  for (int src_i = start_i; src_i <= end_i; /* see loop bottom */ ) {
    copy_entry_to(from_cp, src_i, to_cp, dest_i, CHECK);

    switch (from_cp->tag_at(src_i).value()) {
    case JVM_CONSTANT_Double:
    case JVM_CONSTANT_Long:
      // double and long take two constant pool entries
      src_i += 2;
      dest_i += 2;
      break;

    default:
      // all others take one constant pool entry
      src_i++;
      dest_i++;
      break;
    }
  }
  copy_operands(from_cp, to_cp, CHECK);

} // end copy_cp_to_impl()


// Copy this constant pool's entry at from_i to the constant pool
// to_cp's entry at to_i.
void ConstantPool::copy_entry_to(constantPoolHandle from_cp, int from_i,
                                        constantPoolHandle to_cp, int to_i,
                                        TRAPS) {

  int tag = from_cp->tag_at(from_i).value();
  switch (tag) {
  case JVM_CONSTANT_Class:
  {
    Klass* k = from_cp->klass_at(from_i, CHECK);
    to_cp->klass_at_put(to_i, k);
  } break;

  case JVM_CONSTANT_ClassIndex:
  {
    jint ki = from_cp->klass_index_at(from_i);
    to_cp->klass_index_at_put(to_i, ki);
  } break;

  case JVM_CONSTANT_Double:
  {
    jdouble d = from_cp->double_at(from_i);
    to_cp->double_at_put(to_i, d);
    // double takes two constant pool entries so init second entry's tag
    to_cp->tag_at_put(to_i + 1, JVM_CONSTANT_Invalid);
  } break;

  case JVM_CONSTANT_Fieldref:
  {
    int class_index = from_cp->uncached_klass_ref_index_at(from_i);
    int name_and_type_index = from_cp->uncached_name_and_type_ref_index_at(from_i);
    to_cp->field_at_put(to_i, class_index, name_and_type_index);
  } break;

  case JVM_CONSTANT_Float:
  {
    jfloat f = from_cp->float_at(from_i);
    to_cp->float_at_put(to_i, f);
  } break;

  case JVM_CONSTANT_Integer:
  {
    jint i = from_cp->int_at(from_i);
    to_cp->int_at_put(to_i, i);
  } break;

  case JVM_CONSTANT_InterfaceMethodref:
  {
    int class_index = from_cp->uncached_klass_ref_index_at(from_i);
    int name_and_type_index = from_cp->uncached_name_and_type_ref_index_at(from_i);
    to_cp->interface_method_at_put(to_i, class_index, name_and_type_index);
  } break;

  case JVM_CONSTANT_Long:
  {
    jlong l = from_cp->long_at(from_i);
    to_cp->long_at_put(to_i, l);
    // long takes two constant pool entries so init second entry's tag
    to_cp->tag_at_put(to_i + 1, JVM_CONSTANT_Invalid);
  } break;

  case JVM_CONSTANT_Methodref:
  {
    int class_index = from_cp->uncached_klass_ref_index_at(from_i);
    int name_and_type_index = from_cp->uncached_name_and_type_ref_index_at(from_i);
    to_cp->method_at_put(to_i, class_index, name_and_type_index);
  } break;

  case JVM_CONSTANT_NameAndType:
  {
    int name_ref_index = from_cp->name_ref_index_at(from_i);
    int signature_ref_index = from_cp->signature_ref_index_at(from_i);
    to_cp->name_and_type_at_put(to_i, name_ref_index, signature_ref_index);
  } break;

  case JVM_CONSTANT_StringIndex:
  {
    jint si = from_cp->string_index_at(from_i);
    to_cp->string_index_at_put(to_i, si);
  } break;

  case JVM_CONSTANT_UnresolvedClass:
  case JVM_CONSTANT_UnresolvedClassInError:
  {
    // Can be resolved after checking tag, so check the slot first.
    CPSlot entry = from_cp->slot_at(from_i);
    if (entry.is_resolved()) {
      assert(entry.get_klass()->is_klass(), "must be");
      // Already resolved
      to_cp->klass_at_put(to_i, entry.get_klass());
    } else {
      to_cp->unresolved_klass_at_put(to_i, entry.get_symbol());
    }
  } break;

  case JVM_CONSTANT_String:
  {
    Symbol* s = from_cp->unresolved_string_at(from_i);
    to_cp->unresolved_string_at_put(to_i, s);
  } break;

  case JVM_CONSTANT_Utf8:
  {
    Symbol* s = from_cp->symbol_at(from_i);
    // Need to increase refcount, the old one will be thrown away and deferenced
    s->increment_refcount();
    to_cp->symbol_at_put(to_i, s);
  } break;

  case JVM_CONSTANT_MethodType:
  case JVM_CONSTANT_MethodTypeInError:
  {
    jint k = from_cp->method_type_index_at_error_ok(from_i);
    to_cp->method_type_index_at_put(to_i, k);
  } break;

  case JVM_CONSTANT_MethodHandle:
  case JVM_CONSTANT_MethodHandleInError:
  {
    int k1 = from_cp->method_handle_ref_kind_at_error_ok(from_i);
    int k2 = from_cp->method_handle_index_at_error_ok(from_i);
    to_cp->method_handle_index_at_put(to_i, k1, k2);
  } break;

  case JVM_CONSTANT_InvokeDynamic:
  {
    int k1 = from_cp->invoke_dynamic_bootstrap_specifier_index(from_i);
    int k2 = from_cp->invoke_dynamic_name_and_type_ref_index_at(from_i);
    k1 += operand_array_length(to_cp->operands());  // to_cp might already have operands
    to_cp->invoke_dynamic_at_put(to_i, k1, k2);
  } break;

  // Invalid is used as the tag for the second constant pool entry
  // occupied by JVM_CONSTANT_Double or JVM_CONSTANT_Long. It should
  // not be seen by itself.
  case JVM_CONSTANT_Invalid: // fall through

  default:
  {
    ShouldNotReachHere();
  } break;
  }
} // end copy_entry_to()


// Search constant pool search_cp for an entry that matches this
// constant pool's entry at pattern_i. Returns the index of a
// matching entry or zero (0) if there is no matching entry.
int ConstantPool::find_matching_entry(int pattern_i,
      constantPoolHandle search_cp, TRAPS) {

  // index zero (0) is not used
  for (int i = 1; i < search_cp->length(); i++) {
    bool found = compare_entry_to(pattern_i, search_cp, i, CHECK_0);
    if (found) {
      return i;
    }
  }

  return 0;  // entry not found; return unused index zero (0)
} // end find_matching_entry()


// Compare this constant pool's bootstrap specifier at idx1 to the constant pool
// cp2's bootstrap specifier at idx2.
bool ConstantPool::compare_operand_to(int idx1, constantPoolHandle cp2, int idx2, TRAPS) {
  int k1 = operand_bootstrap_method_ref_index_at(idx1);
  int k2 = cp2->operand_bootstrap_method_ref_index_at(idx2);
  bool match = compare_entry_to(k1, cp2, k2, CHECK_false);

  if (!match) {
    return false;
  }
  int argc = operand_argument_count_at(idx1);
  if (argc == cp2->operand_argument_count_at(idx2)) {
    for (int j = 0; j < argc; j++) {
      k1 = operand_argument_index_at(idx1, j);
      k2 = cp2->operand_argument_index_at(idx2, j);
      match = compare_entry_to(k1, cp2, k2, CHECK_false);
      if (!match) {
        return false;
      }
    }
    return true;           // got through loop; all elements equal
  }
  return false;
} // end compare_operand_to()

// Search constant pool search_cp for a bootstrap specifier that matches
// this constant pool's bootstrap specifier at pattern_i index.
// Return the index of a matching bootstrap specifier or (-1) if there is no match.
int ConstantPool::find_matching_operand(int pattern_i,
                    constantPoolHandle search_cp, int search_len, TRAPS) {
  for (int i = 0; i < search_len; i++) {
    bool found = compare_operand_to(pattern_i, search_cp, i, CHECK_(-1));
    if (found) {
      return i;
    }
  }
  return -1;  // bootstrap specifier not found; return unused index (-1)
} // end find_matching_operand()


#ifndef PRODUCT

const char* ConstantPool::printable_name_at(int which) {

  constantTag tag = tag_at(which);

  if (tag.is_string()) {
    return string_at_noresolve(which);
  } else if (tag.is_klass() || tag.is_unresolved_klass()) {
    return klass_name_at(which)->as_C_string();
  } else if (tag.is_symbol()) {
    return symbol_at(which)->as_C_string();
  }
  return "";
}

#endif // PRODUCT


// JVMTI GetConstantPool support

// For debugging of constant pool
const bool debug_cpool = false;

#define DBG(code) do { if (debug_cpool) { (code); } } while(0)

static void print_cpool_bytes(jint cnt, u1 *bytes) {
  const char* WARN_MSG = "Must not be such entry!";
  jint size = 0;
  u2   idx1, idx2;

  for (jint idx = 1; idx < cnt; idx++) {
    jint ent_size = 0;
    u1   tag  = *bytes++;
    size++;                       // count tag

    printf("const #%03d, tag: %02d ", idx, tag);
    switch(tag) {
      case JVM_CONSTANT_Invalid: {
        printf("Invalid");
        break;
      }
      case JVM_CONSTANT_Unicode: {
        printf("Unicode      %s", WARN_MSG);
        break;
      }
      case JVM_CONSTANT_Utf8: {
        u2 len = Bytes::get_Java_u2(bytes);
        char str[128];
        if (len > 127) {
           len = 127;
        }
        strncpy(str, (char *) (bytes+2), len);
        str[len] = '\0';
        printf("Utf8          \"%s\"", str);
        ent_size = 2 + len;
        break;
      }
      case JVM_CONSTANT_Integer: {
        u4 val = Bytes::get_Java_u4(bytes);
        printf("int          %d", *(int *) &val);
        ent_size = 4;
        break;
      }
      case JVM_CONSTANT_Float: {
        u4 val = Bytes::get_Java_u4(bytes);
        printf("float        %5.3ff", *(float *) &val);
        ent_size = 4;
        break;
      }
      case JVM_CONSTANT_Long: {
        u8 val = Bytes::get_Java_u8(bytes);
        printf("long         "INT64_FORMAT, (int64_t) *(jlong *) &val);
        ent_size = 8;
        idx++; // Long takes two cpool slots
        break;
      }
      case JVM_CONSTANT_Double: {
        u8 val = Bytes::get_Java_u8(bytes);
        printf("double       %5.3fd", *(jdouble *)&val);
        ent_size = 8;
        idx++; // Double takes two cpool slots
        break;
      }
      case JVM_CONSTANT_Class: {
        idx1 = Bytes::get_Java_u2(bytes);
        printf("class        #%03d", idx1);
        ent_size = 2;
        break;
      }
      case JVM_CONSTANT_String: {
        idx1 = Bytes::get_Java_u2(bytes);
        printf("String       #%03d", idx1);
        ent_size = 2;
        break;
      }
      case JVM_CONSTANT_Fieldref: {
        idx1 = Bytes::get_Java_u2(bytes);
        idx2 = Bytes::get_Java_u2(bytes+2);
        printf("Field        #%03d, #%03d", (int) idx1, (int) idx2);
        ent_size = 4;
        break;
      }
      case JVM_CONSTANT_Methodref: {
        idx1 = Bytes::get_Java_u2(bytes);
        idx2 = Bytes::get_Java_u2(bytes+2);
        printf("Method       #%03d, #%03d", idx1, idx2);
        ent_size = 4;
        break;
      }
      case JVM_CONSTANT_InterfaceMethodref: {
        idx1 = Bytes::get_Java_u2(bytes);
        idx2 = Bytes::get_Java_u2(bytes+2);
        printf("InterfMethod #%03d, #%03d", idx1, idx2);
        ent_size = 4;
        break;
      }
      case JVM_CONSTANT_NameAndType: {
        idx1 = Bytes::get_Java_u2(bytes);
        idx2 = Bytes::get_Java_u2(bytes+2);
        printf("NameAndType  #%03d, #%03d", idx1, idx2);
        ent_size = 4;
        break;
      }
      case JVM_CONSTANT_ClassIndex: {
        printf("ClassIndex  %s", WARN_MSG);
        break;
      }
      case JVM_CONSTANT_UnresolvedClass: {
        printf("UnresolvedClass: %s", WARN_MSG);
        break;
      }
      case JVM_CONSTANT_UnresolvedClassInError: {
        printf("UnresolvedClassInErr: %s", WARN_MSG);
        break;
      }
      case JVM_CONSTANT_StringIndex: {
        printf("StringIndex: %s", WARN_MSG);
        break;
      }
    }
    printf(";\n");
    bytes += ent_size;
    size  += ent_size;
  }
  printf("Cpool size: %d\n", size);
  fflush(0);
  return;
} /* end print_cpool_bytes */


// Returns size of constant pool entry.
jint ConstantPool::cpool_entry_size(jint idx) {
  switch(tag_at(idx).value()) {
    case JVM_CONSTANT_Invalid:
    case JVM_CONSTANT_Unicode:
      return 1;

    case JVM_CONSTANT_Utf8:
      return 3 + symbol_at(idx)->utf8_length();

    case JVM_CONSTANT_Class:
    case JVM_CONSTANT_String:
    case JVM_CONSTANT_ClassIndex:
    case JVM_CONSTANT_UnresolvedClass:
    case JVM_CONSTANT_UnresolvedClassInError:
    case JVM_CONSTANT_StringIndex:
    case JVM_CONSTANT_MethodType:
    case JVM_CONSTANT_MethodTypeInError:
      return 3;

    case JVM_CONSTANT_MethodHandle:
    case JVM_CONSTANT_MethodHandleInError:
      return 4; //tag, ref_kind, ref_index

    case JVM_CONSTANT_Integer:
    case JVM_CONSTANT_Float:
    case JVM_CONSTANT_Fieldref:
    case JVM_CONSTANT_Methodref:
    case JVM_CONSTANT_InterfaceMethodref:
    case JVM_CONSTANT_NameAndType:
      return 5;

    case JVM_CONSTANT_InvokeDynamic:
      // u1 tag, u2 bsm, u2 nt
      return 5;

    case JVM_CONSTANT_Long:
    case JVM_CONSTANT_Double:
      return 9;
  }
  assert(false, "cpool_entry_size: Invalid constant pool entry tag");
  return 1;
} /* end cpool_entry_size */


// SymbolHashMap is used to find a constant pool index from a string.
// This function fills in SymbolHashMaps, one for utf8s and one for
// class names, returns size of the cpool raw bytes.
jint ConstantPool::hash_entries_to(SymbolHashMap *symmap,
                                          SymbolHashMap *classmap) {
  jint size = 0;

  for (u2 idx = 1; idx < length(); idx++) {
    u2 tag = tag_at(idx).value();
    size += cpool_entry_size(idx);

    switch(tag) {
      case JVM_CONSTANT_Utf8: {
        Symbol* sym = symbol_at(idx);
        symmap->add_entry(sym, idx);
        DBG(printf("adding symbol entry %s = %d\n", sym->as_utf8(), idx));
        break;
      }
      case JVM_CONSTANT_Class:
      case JVM_CONSTANT_UnresolvedClass:
      case JVM_CONSTANT_UnresolvedClassInError: {
        Symbol* sym = klass_name_at(idx);
        classmap->add_entry(sym, idx);
        DBG(printf("adding class entry %s = %d\n", sym->as_utf8(), idx));
        break;
      }
      case JVM_CONSTANT_Long:
      case JVM_CONSTANT_Double: {
        idx++; // Both Long and Double take two cpool slots
        break;
      }
    }
  }
  return size;
} /* end hash_utf8_entries_to */


// Copy cpool bytes.
// Returns:
//    0, in case of OutOfMemoryError
//   -1, in case of internal error
//  > 0, count of the raw cpool bytes that have been copied
int ConstantPool::copy_cpool_bytes(int cpool_size,
                                          SymbolHashMap* tbl,
                                          unsigned char *bytes) {
  u2   idx1, idx2;
  jint size  = 0;
  jint cnt   = length();
  unsigned char *start_bytes = bytes;

  for (jint idx = 1; idx < cnt; idx++) {
    u1   tag      = tag_at(idx).value();
    jint ent_size = cpool_entry_size(idx);

    assert(size + ent_size <= cpool_size, "Size mismatch");

    *bytes = tag;
    DBG(printf("#%03hd tag=%03hd, ", idx, tag));
    switch(tag) {
      case JVM_CONSTANT_Invalid: {
        DBG(printf("JVM_CONSTANT_Invalid"));
        break;
      }
      case JVM_CONSTANT_Unicode: {
        assert(false, "Wrong constant pool tag: JVM_CONSTANT_Unicode");
        DBG(printf("JVM_CONSTANT_Unicode"));
        break;
      }
      case JVM_CONSTANT_Utf8: {
        Symbol* sym = symbol_at(idx);
        char*     str = sym->as_utf8();
        // Warning! It's crashing on x86 with len = sym->utf8_length()
        int       len = (int) strlen(str);
        Bytes::put_Java_u2((address) (bytes+1), (u2) len);
        for (int i = 0; i < len; i++) {
            bytes[3+i] = (u1) str[i];
        }
        DBG(printf("JVM_CONSTANT_Utf8: %s ", str));
        break;
      }
      case JVM_CONSTANT_Integer: {
        jint val = int_at(idx);
        Bytes::put_Java_u4((address) (bytes+1), *(u4*)&val);
        break;
      }
      case JVM_CONSTANT_Float: {
        jfloat val = float_at(idx);
        Bytes::put_Java_u4((address) (bytes+1), *(u4*)&val);
        break;
      }
      case JVM_CONSTANT_Long: {
        jlong val = long_at(idx);
        Bytes::put_Java_u8((address) (bytes+1), *(u8*)&val);
        idx++;             // Long takes two cpool slots
        break;
      }
      case JVM_CONSTANT_Double: {
        jdouble val = double_at(idx);
        Bytes::put_Java_u8((address) (bytes+1), *(u8*)&val);
        idx++;             // Double takes two cpool slots
        break;
      }
      case JVM_CONSTANT_Class:
      case JVM_CONSTANT_UnresolvedClass:
      case JVM_CONSTANT_UnresolvedClassInError: {
        *bytes = JVM_CONSTANT_Class;
        Symbol* sym = klass_name_at(idx);
        idx1 = tbl->symbol_to_value(sym);
        assert(idx1 != 0, "Have not found a hashtable entry");
        Bytes::put_Java_u2((address) (bytes+1), idx1);
        DBG(printf("JVM_CONSTANT_Class: idx=#%03hd, %s", idx1, sym->as_utf8()));
        break;
      }
      case JVM_CONSTANT_String: {
        *bytes = JVM_CONSTANT_String;
        Symbol* sym = unresolved_string_at(idx);
        idx1 = tbl->symbol_to_value(sym);
        assert(idx1 != 0, "Have not found a hashtable entry");
        Bytes::put_Java_u2((address) (bytes+1), idx1);
        DBG(printf("JVM_CONSTANT_String: idx=#%03hd, %s", idx1, sym->as_utf8()));
        break;
      }
      case JVM_CONSTANT_Fieldref:
      case JVM_CONSTANT_Methodref:
      case JVM_CONSTANT_InterfaceMethodref: {
        idx1 = uncached_klass_ref_index_at(idx);
        idx2 = uncached_name_and_type_ref_index_at(idx);
        Bytes::put_Java_u2((address) (bytes+1), idx1);
        Bytes::put_Java_u2((address) (bytes+3), idx2);
        DBG(printf("JVM_CONSTANT_Methodref: %hd %hd", idx1, idx2));
        break;
      }
      case JVM_CONSTANT_NameAndType: {
        idx1 = name_ref_index_at(idx);
        idx2 = signature_ref_index_at(idx);
        Bytes::put_Java_u2((address) (bytes+1), idx1);
        Bytes::put_Java_u2((address) (bytes+3), idx2);
        DBG(printf("JVM_CONSTANT_NameAndType: %hd %hd", idx1, idx2));
        break;
      }
      case JVM_CONSTANT_ClassIndex: {
        *bytes = JVM_CONSTANT_Class;
        idx1 = klass_index_at(idx);
        Bytes::put_Java_u2((address) (bytes+1), idx1);
        DBG(printf("JVM_CONSTANT_ClassIndex: %hd", idx1));
        break;
      }
      case JVM_CONSTANT_StringIndex: {
        *bytes = JVM_CONSTANT_String;
        idx1 = string_index_at(idx);
        Bytes::put_Java_u2((address) (bytes+1), idx1);
        DBG(printf("JVM_CONSTANT_StringIndex: %hd", idx1));
        break;
      }
      case JVM_CONSTANT_MethodHandle:
      case JVM_CONSTANT_MethodHandleInError: {
        *bytes = JVM_CONSTANT_MethodHandle;
        int kind = method_handle_ref_kind_at_error_ok(idx);
        idx1 = method_handle_index_at_error_ok(idx);
        *(bytes+1) = (unsigned char) kind;
        Bytes::put_Java_u2((address) (bytes+2), idx1);
        DBG(printf("JVM_CONSTANT_MethodHandle: %d %hd", kind, idx1));
        break;
      }
      case JVM_CONSTANT_MethodType:
      case JVM_CONSTANT_MethodTypeInError: {
        *bytes = JVM_CONSTANT_MethodType;
        idx1 = method_type_index_at_error_ok(idx);
        Bytes::put_Java_u2((address) (bytes+1), idx1);
        DBG(printf("JVM_CONSTANT_MethodType: %hd", idx1));
        break;
      }
      case JVM_CONSTANT_InvokeDynamic: {
        *bytes = tag;
        idx1 = extract_low_short_from_int(*int_at_addr(idx));
        idx2 = extract_high_short_from_int(*int_at_addr(idx));
        assert(idx2 == invoke_dynamic_name_and_type_ref_index_at(idx), "correct half of u4");
        Bytes::put_Java_u2((address) (bytes+1), idx1);
        Bytes::put_Java_u2((address) (bytes+3), idx2);
        DBG(printf("JVM_CONSTANT_InvokeDynamic: %hd %hd", idx1, idx2));
        break;
      }
    }
    DBG(printf("\n"));
    bytes += ent_size;
    size  += ent_size;
  }
  assert(size == cpool_size, "Size mismatch");

  // Keep temorarily for debugging until it's stable.
  DBG(print_cpool_bytes(cnt, start_bytes));
  return (int)(bytes - start_bytes);
} /* end copy_cpool_bytes */

#undef DBG


void ConstantPool::set_on_stack(const bool value) {
  D_WARN_Unimplement;
}

// JSR 292 support for patching constant pool oops after the class is linked and
// the oop array for resolved references are created.
// We can't do this during classfile parsing, which is how the other indexes are
// patched.  The other patches are applied early for some error checking
// so only defer the pseudo_strings.
void ConstantPool::patch_resolved_references(
                                            GrowableArray<Handle>* cp_patches) {
  assert(EnableInvokeDynamic, "");
  for (int index = 1; index < cp_patches->length(); index++) { // Index 0 is unused
    Handle patch = cp_patches->at(index);
    if (patch.not_null()) {
      assert (tag_at(index).is_string(), "should only be string left");
      // Patching a string means pre-resolving it.
      // The spelling in the constant pool is ignored.
      // The constant reference may be any object whatever.
      // If it is not a real interned string, the constant is referred
      // to as a "pseudo-string", and must be presented to the CP
      // explicitly, because it may require scavenging.
      int obj_index = cp_to_object_index(index);
      pseudo_string_at_put(index, obj_index, patch());
      DEBUG_ONLY(cp_patches->at_put(index, Handle());)
    }
  }
#ifdef ASSERT
  // Ensure that all the patches have been used.
  for (int index = 0; index < cp_patches->length(); index++) {
    assert(cp_patches->at(index).is_null(),
           err_msg("Unused constant pool patch at %d in class file %s",
                   index,
                   pool_holder()->external_name()));
  }
#endif // ASSERT
}

#ifndef PRODUCT

// CompileTheWorld support. Preload all classes loaded references in the passed in constantpool
void ConstantPool::preload_and_initialize_all_classes(ConstantPool* obj, TRAPS) {
  guarantee(obj->is_constantPool(), "object must be constant pool");
  constantPoolHandle cp(THREAD, (ConstantPool*)obj);
  guarantee(cp->pool_holder() != NULL, "must be fully loaded");

  for (int i = 0; i< cp->length();  i++) {
    if (cp->tag_at(i).is_unresolved_klass()) {
      // This will force loading of the class
      Klass* klass = cp->klass_at(i, CHECK);
      if (klass->oop_is_instance()) {
        // Force initialization of class
        InstanceKlass::cast(klass)->initialize(CHECK);
      }
    }
  }
}

#endif


// Printing

void ConstantPool::print_on(outputStream* st) const {
  EXCEPTION_MARK;
  assert(is_constantPool(), "must be constantPool");
  st->print_cr("%s", internal_name());
  if (flags() != 0) {
    st->print(" - flags: 0x%x", flags());
    if (has_preresolution()) st->print(" has_preresolution");
    if (on_stack()) st->print(" on_stack");
    st->cr();
  }
  if (pool_holder() != NULL) {
    st->print_cr(" - holder: " INTPTR_FORMAT, pool_holder());
  }
  st->print_cr(" - cache: " INTPTR_FORMAT, cache());
  st->print_cr(" - resolved_references: " INTPTR_FORMAT, (void *)resolved_references());
  st->print_cr(" - reference_map: " INTPTR_FORMAT, reference_map());

  for (int index = 1; index < length(); index++) {      // Index 0 is unused
    ((ConstantPool*)this)->print_entry_on(index, st);
    switch (tag_at(index).value()) {
      case JVM_CONSTANT_Long :
      case JVM_CONSTANT_Double :
        index++;   // Skip entry following eigth-byte constant
    }

  }
  st->cr();
}

// Print one constant pool entry
void ConstantPool::print_entry_on(const int index, outputStream* st) {
  EXCEPTION_MARK;
  st->print(" - %3d : ", index);
  tag_at(index).print_on(st);
  st->print(" : ");
  switch (tag_at(index).value()) {
    case JVM_CONSTANT_Class :
      { Klass* k = klass_at(index, CATCH);
        guarantee(k != NULL, "need klass");
        k->print_value_on(st);
        st->print(" {0x%lx}", (address)k);
      }
      break;
    case JVM_CONSTANT_Fieldref :
    case JVM_CONSTANT_Methodref :
    case JVM_CONSTANT_InterfaceMethodref :
      st->print("klass_index=%d", uncached_klass_ref_index_at(index));
      st->print(" name_and_type_index=%d", uncached_name_and_type_ref_index_at(index));
      break;
    case JVM_CONSTANT_String :
      if (is_pseudo_string_at(index)) {
        oop anObj = pseudo_string_at(index);
        anObj->print_value_on(st);
        st->print(" {0x%lx}", (address)anObj);
      } else {
        unresolved_string_at(index)->print_value_on(st);
      }
      break;
    case JVM_CONSTANT_Integer :
      st->print("%d", int_at(index));
      break;
    case JVM_CONSTANT_Float :
      st->print("%f", float_at(index));
      break;
    case JVM_CONSTANT_Long :
      st->print_jlong(long_at(index));
      break;
    case JVM_CONSTANT_Double :
      st->print("%lf", double_at(index));
      break;
    case JVM_CONSTANT_NameAndType :
      st->print("name_index=%d", name_ref_index_at(index));
      st->print(" signature_index=%d", signature_ref_index_at(index));
      break;
    case JVM_CONSTANT_Utf8 :
      symbol_at(index)->print_value_on(st);
      break;
    case JVM_CONSTANT_UnresolvedClass :               // fall-through
    case JVM_CONSTANT_UnresolvedClassInError: {
      // unresolved_klass_at requires lock or safe world.
      CPSlot entry = slot_at(index);
      if (entry.is_resolved()) {
        entry.get_klass()->print_value_on(st);
      } else {
        entry.get_symbol()->print_value_on(st);
      }
      }
      break;
    case JVM_CONSTANT_MethodHandle :
    case JVM_CONSTANT_MethodHandleInError :
      st->print("ref_kind=%d", method_handle_ref_kind_at_error_ok(index));
      st->print(" ref_index=%d", method_handle_index_at_error_ok(index));
      break;
    case JVM_CONSTANT_MethodType :
    case JVM_CONSTANT_MethodTypeInError :
      st->print("signature_index=%d", method_type_index_at_error_ok(index));
      break;
    case JVM_CONSTANT_InvokeDynamic :
      {
        st->print("bootstrap_method_index=%d", invoke_dynamic_bootstrap_method_ref_index_at(index));
        st->print(" name_and_type_index=%d", invoke_dynamic_name_and_type_ref_index_at(index));
        int argc = invoke_dynamic_argument_count_at(index);
        if (argc > 0) {
          for (int arg_i = 0; arg_i < argc; arg_i++) {
            int arg = invoke_dynamic_argument_index_at(index, arg_i);
            st->print((arg_i == 0 ? " arguments={%d" : ", %d"), arg);
          }
          st->print("}");
        }
      }
      break;
    default:
      ShouldNotReachHere();
      break;
  }
  st->cr();
}

void ConstantPool::print_value_on(outputStream* st) const {
  assert(is_constantPool(), "must be constantPool");
  st->print("constant pool [%d]", length());
  if (has_preresolution()) st->print("/preresolution");
  if (operands() != NULL)  st->print("/operands[%d]", operands()->length());
  print_address_on(st);
  st->print(" for ");
  pool_holder()->print_value_on(st);
  if (pool_holder() != NULL) {
    bool extra = (pool_holder()->constants() != this);
    if (extra)  st->print(" (extra)");
  }
  if (cache() != NULL) {
    st->print(" cache=" PTR_FORMAT, cache());
  }
}

#if INCLUDE_SERVICES
// Size Statistics
void ConstantPool::collect_statistics(KlassSizeStats *sz) const {
  D_WARN_Unimplement;
}
#endif // INCLUDE_SERVICES

// Verification

void ConstantPool::verify_on(outputStream* st) {
  guarantee(is_constantPool(), "object must be constant pool");
  for (int i = 0; i< length();  i++) {
    constantTag tag = tag_at(i);
    CPSlot entry = slot_at(i);
    if (tag.is_klass()) {
      if (entry.is_resolved()) {
        guarantee(entry.get_klass()->is_klass(),    "should be klass");
      }
    } else if (tag.is_unresolved_klass()) {
      if (entry.is_resolved()) {
        guarantee(entry.get_klass()->is_klass(),    "should be klass");
      }
    } else if (tag.is_symbol()) {
      guarantee(entry.get_symbol()->refcount() != 0, "should have nonzero reference count");
    } else if (tag.is_string()) {
      guarantee(entry.get_symbol()->refcount() != 0, "should have nonzero reference count");
    }
  }
  if (cache() != NULL) {
    // Note: cache() can be NULL before a class is completely setup or
    // in temporary constant pools used during constant pool merging
    guarantee(cache()->is_constantPoolCache(), "should be constant pool cache");
  }
  if (pool_holder() != NULL) {
    // Note: pool_holder() can be NULL in temporary constant pools
    // used during constant pool merging
    guarantee(pool_holder()->is_klass(),    "should be klass");
  }
}


void SymbolHashMap::add_entry(Symbol* sym, u2 value) {
  char *str = sym->as_utf8();
  unsigned int hash = compute_hash(str, sym->utf8_length());
  unsigned int index = hash % table_size();

  // check if already in map
  // we prefer the first entry since it is more likely to be what was used in
  // the class file
  for (SymbolHashMapEntry *en = bucket(index); en != NULL; en = en->next()) {
    assert(en->symbol() != NULL, "SymbolHashMapEntry symbol is NULL");
    if (en->hash() == hash && en->symbol() == sym) {
        return;  // already there
    }
  }

  SymbolHashMapEntry* entry = new SymbolHashMapEntry(hash, sym, value);
  entry->set_next(bucket(index));
  _buckets[index].set_entry(entry);
  assert(entry->symbol() != NULL, "SymbolHashMapEntry symbol is NULL");
}

SymbolHashMapEntry* SymbolHashMap::find_entry(Symbol* sym) {
  assert(sym != NULL, "SymbolHashMap::find_entry - symbol is NULL");
  char *str = sym->as_utf8();
  int   len = sym->utf8_length();
  unsigned int hash = SymbolHashMap::compute_hash(str, len);
  unsigned int index = hash % table_size();
  for (SymbolHashMapEntry *en = bucket(index); en != NULL; en = en->next()) {
    assert(en->symbol() != NULL, "SymbolHashMapEntry symbol is NULL");
    if (en->hash() == hash && en->symbol() == sym) {
      return en;
    }
  }
  return NULL;
}
