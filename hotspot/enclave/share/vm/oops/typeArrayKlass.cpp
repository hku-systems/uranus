/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
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
#include "memory/universe.hpp"
#include "memory/universe.inline.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/klass.inline.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/oop.inline.hpp"
#include "oops/typeArrayKlass.hpp"
#include "oops/typeArrayOop.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "utilities/macros.hpp"

bool TypeArrayKlass::compute_is_subtype_of(Klass* k) {
  if (!k->oop_is_typeArray()) {
    return ArrayKlass::compute_is_subtype_of(k);
  }

  TypeArrayKlass* tak = TypeArrayKlass::cast(k);
  if (dimension() != tak->dimension()) return false;

  return element_type() == tak->element_type();
}

TypeArrayKlass* TypeArrayKlass::create_klass(BasicType type,
                                      const char* name_str, TRAPS) {
  D_WARN_Unimplement;
}

TypeArrayKlass* TypeArrayKlass::allocate(ClassLoaderData* loader_data, BasicType type, Symbol* name, TRAPS) {
  assert(TypeArrayKlass::header_size() <= InstanceKlass::header_size(),
      "array klasses must be same size as InstanceKlass");

  int size = ArrayKlass::static_size(TypeArrayKlass::header_size());

  return new (loader_data, size, THREAD) TypeArrayKlass(type, name);
}

TypeArrayKlass::TypeArrayKlass(BasicType type, Symbol* name) : ArrayKlass(name) {
  set_layout_helper(array_layout_helper(type));
  assert(oop_is_array(), "sanity");
  assert(oop_is_typeArray(), "sanity");

  set_max_length(arrayOopDesc::max_array_length(type));
  assert(size() >= TypeArrayKlass::header_size(), "bad size");

  set_class_loader_data(ClassLoaderData::the_null_class_loader_data());
}

typeArrayOop TypeArrayKlass::allocate_common(int length, bool do_zero, TRAPS) {
  assert(log2_element_size() >= 0, "bad scale");
  D_WARN_Unimplement;
}

oop TypeArrayKlass::multi_allocate(int rank, jint* last_size, TRAPS) {
  // For typeArrays this is only called for the last dimension
  assert(rank == 1, "just checking");
  int length = *last_size;
  return allocate(length, THREAD);
}


void TypeArrayKlass::copy_array(arrayOop s, int src_pos, arrayOop d, int dst_pos, int length, TRAPS) {
  D_WARN_Unimplement;
}


// create a klass of array holding typeArrays
Klass* TypeArrayKlass::array_klass_impl(bool or_null, int n, TRAPS) {
  D_WARN_Unimplement;
}

Klass* TypeArrayKlass::array_klass_impl(bool or_null, TRAPS) {
  return array_klass_impl(or_null, dimension() +  1, THREAD);
}

int TypeArrayKlass::oop_size(oop obj) const {
  assert(obj->is_typeArray(),"must be a type array");
  typeArrayOop t = typeArrayOop(obj);
  return t->object_size();
}

void TypeArrayKlass::oop_follow_contents(oop obj) {
  assert(obj->is_typeArray(),"must be a type array");
  // Performance tweak: We skip iterating over the klass pointer since we
  // know that Universe::TypeArrayKlass never moves.
}

#if INCLUDE_ALL_GCS
void TypeArrayKlass::oop_follow_contents(ParCompactionManager* cm, oop obj) {
  assert(obj->is_typeArray(),"must be a type array");
  // Performance tweak: We skip iterating over the klass pointer since we
  // know that Universe::TypeArrayKlass never moves.
}
#endif // INCLUDE_ALL_GCS

int TypeArrayKlass::oop_adjust_pointers(oop obj) {
  assert(obj->is_typeArray(),"must be a type array");
  typeArrayOop t = typeArrayOop(obj);
  // Performance tweak: We skip iterating over the klass pointer since we
  // know that Universe::TypeArrayKlass never moves.
  return t->object_size();
}

int TypeArrayKlass::oop_oop_iterate(oop obj, ExtendedOopClosure* blk) {
  assert(obj->is_typeArray(),"must be a type array");
  typeArrayOop t = typeArrayOop(obj);
  // Performance tweak: We skip iterating over the klass pointer since we
  // know that Universe::TypeArrayKlass never moves.
  return t->object_size();
}

int TypeArrayKlass::oop_oop_iterate_m(oop obj, ExtendedOopClosure* blk, MemRegion mr) {
  assert(obj->is_typeArray(),"must be a type array");
  typeArrayOop t = typeArrayOop(obj);
  // Performance tweak: We skip iterating over the klass pointer since we
  // know that Universe::TypeArrayKlass never moves.
  return t->object_size();
}

#if INCLUDE_ALL_GCS
void TypeArrayKlass::oop_push_contents(PSPromotionManager* pm, oop obj) {
  ShouldNotReachHere();
  assert(obj->is_typeArray(),"must be a type array");
}

int
TypeArrayKlass::oop_update_pointers(ParCompactionManager* cm, oop obj) {
  assert(obj->is_typeArray(),"must be a type array");
  return typeArrayOop(obj)->object_size();
}
#endif // INCLUDE_ALL_GCS

void TypeArrayKlass::initialize(TRAPS) {
  // Nothing to do. Having this function is handy since objArrayKlasses can be
  // initialized by calling initialize on their bottom_klass, see ObjArrayKlass::initialize
}

const char* TypeArrayKlass::external_name(BasicType type) {
  switch (type) {
    case T_BOOLEAN: return "[Z";
    case T_CHAR:    return "[C";
    case T_FLOAT:   return "[F";
    case T_DOUBLE:  return "[D";
    case T_BYTE:    return "[B";
    case T_SHORT:   return "[S";
    case T_INT:     return "[I";
    case T_LONG:    return "[J";
    default: ShouldNotReachHere();
  }
  return NULL;
}


// Printing

void TypeArrayKlass::print_on(outputStream* st) const {
#ifndef PRODUCT
  assert(is_klass(), "must be klass");
  print_value_on(st);
  Klass::print_on(st);
#endif //PRODUCT
}

void TypeArrayKlass::print_value_on(outputStream* st) const {
  assert(is_klass(), "must be klass");
  st->print("{type array ");
  switch (element_type()) {
    case T_BOOLEAN: st->print("bool");    break;
    case T_CHAR:    st->print("char");    break;
    case T_FLOAT:   st->print("float");   break;
    case T_DOUBLE:  st->print("double");  break;
    case T_BYTE:    st->print("byte");    break;
    case T_SHORT:   st->print("short");   break;
    case T_INT:     st->print("int");     break;
    case T_LONG:    st->print("long");    break;
    default: ShouldNotReachHere();
  }
  st->print("}");
}

#ifndef PRODUCT

static void print_boolean_array(typeArrayOop ta, int print_len, outputStream* st) {
  for (int index = 0; index < print_len; index++) {
    st->print_cr(" - %3d: %s", index, (ta->bool_at(index) == 0) ? "false" : "true");
  }
}


static void print_char_array(typeArrayOop ta, int print_len, outputStream* st) {
  for (int index = 0; index < print_len; index++) {
    jchar c = ta->char_at(index);
    st->print_cr(" - %3d: %x %c", index, c, isprint(c) ? c : ' ');
  }
}


static void print_float_array(typeArrayOop ta, int print_len, outputStream* st) {
  for (int index = 0; index < print_len; index++) {
    st->print_cr(" - %3d: %g", index, ta->float_at(index));
  }
}


static void print_double_array(typeArrayOop ta, int print_len, outputStream* st) {
  for (int index = 0; index < print_len; index++) {
    st->print_cr(" - %3d: %g", index, ta->double_at(index));
  }
}


static void print_byte_array(typeArrayOop ta, int print_len, outputStream* st) {
  for (int index = 0; index < print_len; index++) {
    jbyte c = ta->byte_at(index);
    st->print_cr(" - %3d: %x %c", index, c, isprint(c) ? c : ' ');
  }
}


static void print_short_array(typeArrayOop ta, int print_len, outputStream* st) {
  for (int index = 0; index < print_len; index++) {
    int v = ta->ushort_at(index);
    st->print_cr(" - %3d: 0x%x\t %d", index, v, v);
  }
}


static void print_int_array(typeArrayOop ta, int print_len, outputStream* st) {
  for (int index = 0; index < print_len; index++) {
    jint v = ta->int_at(index);
    st->print_cr(" - %3d: 0x%x %d", index, v, v);
  }
}


static void print_long_array(typeArrayOop ta, int print_len, outputStream* st) {
  for (int index = 0; index < print_len; index++) {
    jlong v = ta->long_at(index);
    st->print_cr(" - %3d: 0x%x 0x%x", index, high(v), low(v));
  }
}


void TypeArrayKlass::oop_print_on(oop obj, outputStream* st) {
  ArrayKlass::oop_print_on(obj, st);
  typeArrayOop ta = typeArrayOop(obj);
  int print_len = MIN2((intx) ta->length(), MaxElementPrintSize);
  switch (element_type()) {
    case T_BOOLEAN: print_boolean_array(ta, print_len, st); break;
    case T_CHAR:    print_char_array(ta, print_len, st);    break;
    case T_FLOAT:   print_float_array(ta, print_len, st);   break;
    case T_DOUBLE:  print_double_array(ta, print_len, st);  break;
    case T_BYTE:    print_byte_array(ta, print_len, st);    break;
    case T_SHORT:   print_short_array(ta, print_len, st);   break;
    case T_INT:     print_int_array(ta, print_len, st);     break;
    case T_LONG:    print_long_array(ta, print_len, st);    break;
    default: ShouldNotReachHere();
  }
  int remaining = ta->length() - print_len;
  if (remaining > 0) {
    st->print_cr(" - <%d more elements, increase MaxElementPrintSize to print>", remaining);
  }
}

#endif // PRODUCT

const char* TypeArrayKlass::internal_name() const {
  return Klass::external_name();
}
