/*
 * Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include "oops/annotations.hpp"
#include "oops/instanceKlass.hpp"
#include "utilities/ostream.hpp"

// Allocate annotations in metadata area
Annotations* Annotations::allocate(ClassLoaderData* loader_data, TRAPS) {
  return new (loader_data, size(), true, MetaspaceObj::AnnotationType, THREAD) Annotations();
}

// helper
void Annotations::free_contents(ClassLoaderData* loader_data, Array<AnnotationArray*>* p) {
  D_WARN_Unimplement;
}

void Annotations::deallocate_contents(ClassLoaderData* loader_data) {
  D_WARN_Unimplement;
}

// Copy annotations to JVM call or reflection to the java heap.
// The alternative to creating this array and adding to Java heap pressure
// is to have a hashtable of the already created typeArrayOops
typeArrayOop Annotations::make_java_array(AnnotationArray* annotations, TRAPS) {
  D_WARN_Unimplement;
}


void Annotations::print_value_on(outputStream* st) const {
  st->print("Anotations(" INTPTR_FORMAT ")", p2i(this));
}

#if INCLUDE_SERVICES
// Size Statistics

julong Annotations::count_bytes(Array<AnnotationArray*>* p) {
  D_WARN_Unimplement;
}

void Annotations::collect_statistics(KlassSizeStats *sz) const {
  D_WARN_Unimplement;
}
#endif // INCLUDE_SERVICES

#define BULLET  " - "

#ifndef PRODUCT
void Annotations::print_on(outputStream* st) const {
  st->print(BULLET"class_annotations            "); class_annotations()->print_value_on(st);
  st->print(BULLET"fields_annotations           "); fields_annotations()->print_value_on(st);
  st->print(BULLET"class_type_annotations       "); class_type_annotations()->print_value_on(st);
  st->print(BULLET"fields_type_annotations      "); fields_type_annotations()->print_value_on(st);
}
#endif // PRODUCT
