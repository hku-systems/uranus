/*
 * Copyright (c) 2008, 2016, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/symbolTable.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/oopMapCache.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/oopFactory.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/reflection.hpp"
#include "runtime/signature.hpp"
#include "runtime/stubRoutines.hpp"


/*
 * JSR 292 reference implementation: method handles
 * The JDK 7 reference implementation represented method handle
 * combinations as chains.  Each link in the chain had a "vmentry"
 * field which pointed at a bit of assembly code which performed
 * one transformation before dispatching to the next link in the chain.
 *
 * The current reference implementation pushes almost all code generation
 * responsibility to (trusted) Java code.  A method handle contains a
 * pointer to its "LambdaForm", which embodies all details of the method
 * handle's behavior.  The LambdaForm is a normal Java object, managed
 * by a runtime coded in Java.
 */

bool MethodHandles::_enabled = false; // set true after successful native linkage
MethodHandlesAdapterBlob* MethodHandles::_adapter_code = NULL;


void MethodHandles::set_enabled(bool z) {
  if (_enabled != z) {
    guarantee(z && EnableInvokeDynamic, "can only enable once, and only if -XX:+EnableInvokeDynamic");
    _enabled = z;
  }
}

// MemberName support

// import java_lang_invoke_MemberName.*
enum {
  IS_METHOD            = java_lang_invoke_MemberName::MN_IS_METHOD,
  IS_CONSTRUCTOR       = java_lang_invoke_MemberName::MN_IS_CONSTRUCTOR,
  IS_FIELD             = java_lang_invoke_MemberName::MN_IS_FIELD,
  IS_TYPE              = java_lang_invoke_MemberName::MN_IS_TYPE,
  CALLER_SENSITIVE     = java_lang_invoke_MemberName::MN_CALLER_SENSITIVE,
  REFERENCE_KIND_SHIFT = java_lang_invoke_MemberName::MN_REFERENCE_KIND_SHIFT,
  REFERENCE_KIND_MASK  = java_lang_invoke_MemberName::MN_REFERENCE_KIND_MASK,
  SEARCH_SUPERCLASSES  = java_lang_invoke_MemberName::MN_SEARCH_SUPERCLASSES,
  SEARCH_INTERFACES    = java_lang_invoke_MemberName::MN_SEARCH_INTERFACES,
  ALL_KINDS      = IS_METHOD | IS_CONSTRUCTOR | IS_FIELD | IS_TYPE
};


// JVM 2.9 Special Methods:
// A method is signature polymorphic if and only if all of the following conditions hold :
// * It is declared in the java.lang.invoke.MethodHandle class.
// * It has a single formal parameter of type Object[].
// * It has a return type of Object.
// * It has the ACC_VARARGS and ACC_NATIVE flags set.
bool MethodHandles::is_method_handle_invoke_name(Klass* klass, Symbol* name) {
  if (klass == NULL)
    return false;
  // The following test will fail spuriously during bootstrap of MethodHandle itself:
  //    if (klass != SystemDictionary::MethodHandle_klass())
  // Test the name instead:
  if (klass->name() != vmSymbols::java_lang_invoke_MethodHandle())
    return false;
  Symbol* poly_sig = vmSymbols::object_array_object_signature();
  Method* m = InstanceKlass::cast(klass)->find_method(name, poly_sig);
  if (m == NULL)  return false;
  int required = JVM_ACC_NATIVE | JVM_ACC_VARARGS;
  int flags = m->access_flags().as_int();
  return (flags & required) == required;
}


Symbol* MethodHandles::signature_polymorphic_intrinsic_name(vmIntrinsics::ID iid) {
  assert(is_signature_polymorphic_intrinsic(iid), err_msg("iid=%d", iid));
  switch (iid) {
  case vmIntrinsics::_invokeBasic:      return vmSymbols::invokeBasic_name();
  case vmIntrinsics::_linkToVirtual:    return vmSymbols::linkToVirtual_name();
  case vmIntrinsics::_linkToStatic:     return vmSymbols::linkToStatic_name();
  case vmIntrinsics::_linkToSpecial:    return vmSymbols::linkToSpecial_name();
  case vmIntrinsics::_linkToInterface:  return vmSymbols::linkToInterface_name();
  }
  assert(false, "");
  return 0;
}

int MethodHandles::signature_polymorphic_intrinsic_ref_kind(vmIntrinsics::ID iid) {
  switch (iid) {
  case vmIntrinsics::_invokeBasic:      return 0;
  case vmIntrinsics::_linkToVirtual:    return JVM_REF_invokeVirtual;
  case vmIntrinsics::_linkToStatic:     return JVM_REF_invokeStatic;
  case vmIntrinsics::_linkToSpecial:    return JVM_REF_invokeSpecial;
  case vmIntrinsics::_linkToInterface:  return JVM_REF_invokeInterface;
  }
  assert(false, err_msg("iid=%d", iid));
  return 0;
}

vmIntrinsics::ID MethodHandles::signature_polymorphic_name_id(Symbol* name) {
  vmSymbols::SID name_id = vmSymbols::find_sid(name);
  switch (name_id) {
  // The ID _invokeGeneric stands for all non-static signature-polymorphic methods, except built-ins.
  case vmSymbols::VM_SYMBOL_ENUM_NAME(invoke_name):           return vmIntrinsics::_invokeGeneric;
  // The only built-in non-static signature-polymorphic method is MethodHandle.invokeBasic:
  case vmSymbols::VM_SYMBOL_ENUM_NAME(invokeBasic_name):      return vmIntrinsics::_invokeBasic;

  // There is one static signature-polymorphic method for each JVM invocation mode.
  case vmSymbols::VM_SYMBOL_ENUM_NAME(linkToVirtual_name):    return vmIntrinsics::_linkToVirtual;
  case vmSymbols::VM_SYMBOL_ENUM_NAME(linkToStatic_name):     return vmIntrinsics::_linkToStatic;
  case vmSymbols::VM_SYMBOL_ENUM_NAME(linkToSpecial_name):    return vmIntrinsics::_linkToSpecial;
  case vmSymbols::VM_SYMBOL_ENUM_NAME(linkToInterface_name):  return vmIntrinsics::_linkToInterface;
  }

  // Cover the case of invokeExact and any future variants of invokeFoo.
  Klass* mh_klass = SystemDictionary::well_known_klass(
                              SystemDictionary::WK_KLASS_ENUM_NAME(MethodHandle_klass) );
  if (mh_klass != NULL && is_method_handle_invoke_name(mh_klass, name))
    return vmIntrinsics::_invokeGeneric;

  // Note: The pseudo-intrinsic _compiledLambdaForm is never linked against.
  // Instead it is used to mark lambda forms bound to invokehandle or invokedynamic.
  return vmIntrinsics::_none;
}

vmIntrinsics::ID MethodHandles::signature_polymorphic_name_id(Klass* klass, Symbol* name) {
  if (klass != NULL &&
      klass->name() == vmSymbols::java_lang_invoke_MethodHandle()) {
    vmIntrinsics::ID iid = signature_polymorphic_name_id(name);
    if (iid != vmIntrinsics::_none)
      return iid;
    if (is_method_handle_invoke_name(klass, name))
      return vmIntrinsics::_invokeGeneric;
  }
  return vmIntrinsics::_none;
}


// convert the external string or reflective type to an internal signature
Symbol* MethodHandles::lookup_signature(oop type_str, bool intern_if_not_found, TRAPS) {
  if (java_lang_invoke_MethodType::is_instance(type_str)) {
    return java_lang_invoke_MethodType::as_signature(type_str, intern_if_not_found, CHECK_NULL);
  } else if (java_lang_Class::is_instance(type_str)) {
    return java_lang_Class::as_signature(type_str, false, CHECK_NULL);
  } else if (java_lang_String::is_instance(type_str)) {
    if (intern_if_not_found) {
      return java_lang_String::as_symbol(type_str, CHECK_NULL);
    } else {
      return java_lang_String::as_symbol_or_null(type_str);
    }
  } else {
    THROW_MSG_(vmSymbols::java_lang_InternalError(), "unrecognized type", NULL);
  }
}

static const char OBJ_SIG[] = "Ljava/lang/Object;";
enum { OBJ_SIG_LEN = 18 };

bool MethodHandles::is_basic_type_signature(Symbol* sig) {
  assert(vmSymbols::object_signature()->utf8_length() == (int)OBJ_SIG_LEN, "");
  assert(vmSymbols::object_signature()->equals(OBJ_SIG), "");
  const int len = sig->utf8_length();
  for (int i = 0; i < len; i++) {
    switch (sig->byte_at(i)) {
    case 'L':
      // only java/lang/Object is valid here
      if (sig->index_of_at(i, OBJ_SIG, OBJ_SIG_LEN) != i)
        return false;
      i += OBJ_SIG_LEN-1;  //-1 because of i++ in loop
      continue;
    case '(': case ')': case 'V':
    case 'I': case 'J': case 'F': case 'D':
      continue;
    //case '[':
    //case 'Z': case 'B': case 'C': case 'S':
    default:
      return false;
    }
  }
  return true;
}

Symbol* MethodHandles::lookup_basic_type_signature(Symbol* sig, bool keep_last_arg, TRAPS) {
  Symbol* bsig = NULL;
  if (sig == NULL) {
    return sig;
  } else if (is_basic_type_signature(sig)) {
    sig->increment_refcount();
    return sig;  // that was easy
  } else if (sig->byte_at(0) != '(') {
    BasicType bt = char2type(sig->byte_at(0));
    if (is_subword_type(bt)) {
      bsig = vmSymbols::int_signature();
    } else {
      assert(bt == T_OBJECT || bt == T_ARRAY, "is_basic_type_signature was false");
      bsig = vmSymbols::object_signature();
    }
  } else {
    ResourceMark rm;
    stringStream buffer(128);
    buffer.put('(');
    int arg_pos = 0, keep_arg_pos = -1;
    if (keep_last_arg)
      keep_arg_pos = ArgumentCount(sig).size() - 1;
    for (SignatureStream ss(sig); !ss.is_done(); ss.next()) {
      BasicType bt = ss.type();
      size_t this_arg_pos = buffer.size();
      if (ss.at_return_type()) {
        buffer.put(')');
      }
      if (arg_pos == keep_arg_pos) {
        buffer.write((char*) ss.raw_bytes(),
                     (int)   ss.raw_length());
      } else if (bt == T_OBJECT || bt == T_ARRAY) {
        buffer.write(OBJ_SIG, OBJ_SIG_LEN);
      } else {
        if (is_subword_type(bt))
          bt = T_INT;
        buffer.put(type2char(bt));
      }
      arg_pos++;
    }
    const char* sigstr =       buffer.base();
    int         siglen = (int) buffer.size();
    bsig = SymbolTable::new_symbol(sigstr, siglen, THREAD);
  }
  assert(is_basic_type_signature(bsig) ||
         // detune assert in case the injected argument is not a basic type:
         keep_last_arg, "");
  return bsig;
}

void MethodHandles::print_as_basic_type_signature_on(outputStream* st,
                                                     Symbol* sig,
                                                     bool keep_arrays,
                                                     bool keep_basic_names) {
  st = st ? st : tty;
  int len  = sig->utf8_length();
  int array = 0;
  bool prev_type = false;
  for (int i = 0; i < len; i++) {
    char ch = sig->byte_at(i);
    switch (ch) {
    case '(': case ')':
      prev_type = false;
      st->put(ch);
      continue;
    case '[':
      if (!keep_basic_names && keep_arrays)
        st->put(ch);
      array++;
      continue;
    case 'L':
      {
        if (prev_type)  st->put(',');
        int start = i+1, slash = start;
        while (++i < len && (ch = sig->byte_at(i)) != ';') {
          if (ch == '/' || ch == '.' || ch == '$')  slash = i+1;
        }
        if (slash < i)  start = slash;
        if (!keep_basic_names) {
          st->put('L');
        } else {
          for (int j = start; j < i; j++)
            st->put(sig->byte_at(j));
          prev_type = true;
        }
        break;
      }
    default:
      {
        if (array && char2type(ch) != T_ILLEGAL && !keep_arrays) {
          ch = '[';
          array = 0;
        }
        if (prev_type)  st->put(',');
        const char* n = NULL;
        if (keep_basic_names)
          n = type2name(char2type(ch));
        if (n == NULL) {
          // unknown letter, or we don't want to know its name
          st->put(ch);
        } else {
          st->print("%s", n);
          prev_type = true;
        }
        break;
      }
    }
    // Switch break goes here to take care of array suffix:
    if (prev_type) {
      while (array > 0) {
        st->print("[]");
        --array;
      }
    }
    array = 0;
  }
}



static oop object_java_mirror() {
  return SystemDictionary::Object_klass()->java_mirror();
}

oop MethodHandles::field_name_or_null(Symbol* s) {
  if (s == NULL)  return NULL;
  return StringTable::lookup(s);
}

oop MethodHandles::field_signature_type_or_null(Symbol* s) {
  if (s == NULL)  return NULL;
  BasicType bt = FieldType::basic_type(s);
  if (is_java_primitive(bt)) {
    assert(s->utf8_length() == 1, "");
    return java_lang_Class::primitive_mirror(bt);
  }
  // Here are some more short cuts for common types.
  // They are optional, since reference types can be resolved lazily.
  if (bt == T_OBJECT) {
    if (s == vmSymbols::object_signature()) {
      return object_java_mirror();
    } else if (s == vmSymbols::class_signature()) {
      return SystemDictionary::Class_klass()->java_mirror();
    } else if (s == vmSymbols::string_signature()) {
      return SystemDictionary::String_klass()->java_mirror();
    }
  }
  return NULL;
}
