/*
 * Copyright (c) 1997, 2012, Oracle and/or its affiliates. All rights reserved.
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
#include "interpreter/bytecode.hpp"
#include "oops/constantPool.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/signature.hpp"

// Implementation of Bytecode

// Implementation of Bytecode_tableupswitch

int Bytecode_tableswitch::dest_offset_at(int i) const {
  return get_Java_u4_at(aligned_offset(1 + (3 + i)*jintSize));
}


// Implementation of Bytecode_invoke

void Bytecode_invoke::verify() const {
  assert(is_valid(), "check invoke");
  assert(cpcache() != NULL, "do not call this from verifier or rewriter");
}


Symbol* Bytecode_member_ref::klass() const {
  return constants()->klass_ref_at_noresolve(index());
}


Symbol* Bytecode_member_ref::name() const {
  return constants()->name_ref_at(index());
}


Symbol* Bytecode_member_ref::signature() const {
  return constants()->signature_ref_at(index());
}


BasicType Bytecode_member_ref::result_type() const {
  ResultTypeFinder rts(signature());
  rts.iterate();
  return rts.type();
}


methodHandle Bytecode_invoke::static_target(TRAPS) {
  methodHandle m;
  KlassHandle resolved_klass;
  constantPoolHandle constants(THREAD, this->constants());

  Bytecodes::Code bc = invoke_code();
  // TODO:
//  LinkResolver::resolve_method_statically(m, resolved_klass, bc, constants, index(), CHECK_(methodHandle()));
  return m;
}

Handle Bytecode_invoke::appendix(TRAPS) {
  ConstantPoolCacheEntry* cpce = cpcache_entry();
  if (cpce->has_appendix())
    return Handle(THREAD, cpce->appendix_if_resolved(constants()));
  return Handle();  // usual case
}

int Bytecode_member_ref::index() const {
  // Note:  Rewriter::rewrite changes the Java_u2 of an invokedynamic to a native_u4,
  // at the same time it allocates per-call-site CP cache entries.
  Bytecodes::Code rawc = code();
  if (has_index_u4(rawc))
    return get_index_u4(rawc);
  else
    return get_index_u2_cpcache(rawc);
}

int Bytecode_member_ref::pool_index() const {
  return cpcache_entry()->constant_pool_index();
}

ConstantPoolCacheEntry* Bytecode_member_ref::cpcache_entry() const {
  int index = this->index();
  return cpcache()->entry_at(ConstantPool::decode_cpcache_index(index, true));
}

// Implementation of Bytecode_field

void Bytecode_field::verify() const {
  assert(is_valid(), "check field");
}


// Implementation of Bytecode_loadconstant

int Bytecode_loadconstant::raw_index() const {
  Bytecodes::Code rawc = code();
  assert(rawc != Bytecodes::_wide, "verifier prevents this");
  if (Bytecodes::java_code(rawc) == Bytecodes::_ldc)
    return get_index_u1(rawc);
  else
    return get_index_u2(rawc, false);
}

int Bytecode_loadconstant::pool_index() const {
  int index = raw_index();
  if (has_cache_index()) {
    return _method->constants()->object_to_cp_index(index);
  }
  return index;
}

BasicType Bytecode_loadconstant::result_type() const {
  int index = pool_index();
  constantTag tag = _method->constants()->tag_at(index);
  return tag.basic_type();
}

oop Bytecode_loadconstant::resolve_constant(TRAPS) const {
  assert(_method.not_null(), "must supply method to resolve constant");
  int index = raw_index();
  ConstantPool* constants = _method->constants();
  if (has_cache_index()) {
    return constants->resolve_cached_constant_at(index, THREAD);
  } else {
    return constants->resolve_constant_at(index, THREAD);
  }
}

//------------------------------------------------------------------------------
// Non-product code

#ifndef PRODUCT

void Bytecode_lookupswitch::verify() const {
  switch (Bytecodes::java_code(code())) {
    case Bytecodes::_lookupswitch:
      { int i = number_of_pairs() - 1;
        while (i-- > 0) {
          assert(pair_at(i).match() < pair_at(i+1).match(), "unsorted table entries");
        }
      }
      break;
    default:
      fatal("not a lookupswitch bytecode");
  }
}

void Bytecode_tableswitch::verify() const {
  switch (Bytecodes::java_code(code())) {
    case Bytecodes::_tableswitch:
      { int lo = low_key();
        int hi = high_key();
        assert (hi >= lo, "incorrect hi/lo values in tableswitch");
        int i  = hi - lo - 1 ;
        while (i-- > 0) {
          // no special check needed
        }
      }
      break;
    default:
      fatal("not a tableswitch bytecode");
  }
}

#endif
