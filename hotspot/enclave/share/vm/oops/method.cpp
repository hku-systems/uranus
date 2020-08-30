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
#include "classfile/systemDictionary.hpp"
#include "interpreter/bytecodes.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/oopMapCache.hpp"
#include "oops/constMethod.hpp"
#include "oops/method.hpp"
#include "oops/oop.inline.hpp"
#include "oops/symbol.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "runtime/signature.hpp"
#include "utilities/quickSort.hpp"

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

// Implementation of Method

#define HOOK_INTERFACE_NAME "IntelSGX;"
#define UNHOOK_INTERFACE_NAME "IntelSGXOcall;"
#define FUNCTION_INTERFACE_NAME "IntelSGXFunc;"

address Method::_hook_method_entry_point = NULL;
#include "enclave/sc/EnclaveOcall.h"

enum sgx_call_flag {
    normal_call_flag = 0x00,
    sgx_ecall_flag = 0x01,
    sgx_ocall_flag = 0x02,
    sgx_func_flag = 0x05
} sgx_call_flag_t;


address _hook_point = NULL;

int resolve_flags(const Method* m, int index) {
  Symbol* klassName = m->constants()->symbol_at(index);
  if (klassName != NULL) {
    const char *name = klassName->as_C_string();
    int len = strlen(name);
    if (len >= strlen(HOOK_INTERFACE_NAME) && strcmp(name + len - strlen(HOOK_INTERFACE_NAME), HOOK_INTERFACE_NAME) == 0) {
      return sgx_ecall_flag;
    } else if (len >= strlen(UNHOOK_INTERFACE_NAME) && strcmp(name + len - strlen(UNHOOK_INTERFACE_NAME), UNHOOK_INTERFACE_NAME) == 0) {
      return sgx_ocall_flag;
    } else if (len >= strlen(FUNCTION_INTERFACE_NAME) && strcmp(name + len - strlen(FUNCTION_INTERFACE_NAME), FUNCTION_INTERFACE_NAME) == 0) {
      return sgx_func_flag;
    }
  }
  return normal_call_flag;
}

int is_sgx_interface(const Method* m) {
    AnnotationArray* ar = m->annotations();
    int flags = normal_call_flag;
    if (ar != NULL) {
        int num = ar->at(0) * 256 + ar->at(1);
        int typeIndex = 0;
        if (num <= 2) {
          for (int i = 1;i <= num;i++)
            typeIndex = ar->at(i * 2) * 256 + ar->at(i * 2 + 1);
            flags |= resolve_flags(m, typeIndex);
        }
    }
    return flags;
}

int sgx_hook_func() {
  return 1;
}

Method* Method::allocate(ClassLoaderData* loader_data,
                         int byte_code_size,
                         AccessFlags access_flags,
                         InlineTableSizes* sizes,
                         ConstMethod::MethodType method_type,
                         TRAPS) {
  assert(!access_flags.is_native() || byte_code_size == 0,
         "native methods should not contain byte codes");
  ConstMethod* cm = ConstMethod::allocate(loader_data,
                                          byte_code_size,
                                          sizes,
                                          method_type,
                                          CHECK_NULL);

  int size = Method::size(access_flags.is_native());

  return new (loader_data, size, false, MetaspaceObj::MethodType, THREAD) Method(cm, access_flags, size);
}

Method::Method(ConstMethod* xconst, AccessFlags access_flags, int size) {
  // No_Safepoint_Verifier no_safepoint;
  set_constMethod(xconst);
  set_access_flags(access_flags);
  set_method_size(size);
  set_intrinsic_id(vmIntrinsics::_none);
  set_jfr_towrite(false);
  set_force_inline(false);
  set_hidden(false);
  set_dont_inline(false);
  set_has_injected_profile(false);
  set_method_data(NULL);
  clear_method_counters();
  set_vtable_index(Method::garbage_vtable_index);

  // Fix and bury in Method*
  set_interpreter_entry(NULL); // sets i2i entry and from_int
  set_adapter_entry(NULL);
  clear_code(false /* don't need a lock */); // from_c/from_i get set to c2i/i2i

  if (access_flags.is_native()) {
    clear_native_function();
    set_signature_handler(NULL);
  }

  NOT_PRODUCT(set_compiled_invocation_count(0);)
}

// Release Method*.  The nmethod will be gone when we get here because
// we've walked the code cache.
void Method::deallocate_contents(ClassLoaderData* loader_data) {
  clear_jmethod_id(loader_data);
  MetadataFactory::free_metadata(loader_data, constMethod());
  set_constMethod(NULL);
  MetadataFactory::free_metadata(loader_data, method_data());
  set_method_data(NULL);
  MetadataFactory::free_metadata(loader_data, method_counters());
  clear_method_counters();
  // The nmethod will be gone when we get here.
  if (code() != NULL) _code = NULL;
}

address Method::get_i2c_entry() {
  D_WARN_Unimplement;
}

address Method::get_c2i_entry() {
  D_WARN_Unimplement;
}

address Method::get_c2i_unverified_entry() {
  D_WARN_Unimplement;
}

char* Method::name_and_sig_as_C_string() const {
  return name_and_sig_as_C_string(constants()->pool_holder(), name(), signature());
}

char* Method::name_and_sig_as_C_string(char* buf, int size) const {
  return name_and_sig_as_C_string(constants()->pool_holder(), name(), signature(), buf, size);
}

char* Method::name_and_sig_as_C_string(Klass* klass, Symbol* method_name, Symbol* signature) {
  const char* klass_name = klass->external_name();
  int klass_name_len  = (int)strlen(klass_name);
  int method_name_len = method_name->utf8_length();
  int len             = klass_name_len + 1 + method_name_len + signature->utf8_length();
  char* dest          = NEW_RESOURCE_ARRAY(char, len + 1);
  strcpy(dest, klass_name);
  dest[klass_name_len] = '.';
  strcpy(&dest[klass_name_len + 1], method_name->as_C_string());
  strcpy(&dest[klass_name_len + 1 + method_name_len], signature->as_C_string());
  dest[len] = 0;
  return dest;
}

char* Method::name_and_sig_as_C_string(Klass* klass, Symbol* method_name, Symbol* signature, char* buf, int size) {
  Symbol* klass_name = klass->name();
  klass_name->as_klass_external_name(buf, size);
  int len = (int)strlen(buf);

  if (len < size - 1) {
    buf[len++] = '.';

    method_name->as_C_string(&(buf[len]), size - len);
    len = (int)strlen(buf);

    signature->as_C_string(&(buf[len]), size - len);
  }

  return buf;
}

int Method::fast_exception_handler_bci_for(methodHandle mh, KlassHandle ex_klass, int throw_bci, TRAPS) {
  // exception table holds quadruple entries of the form (beg_bci, end_bci, handler_bci, klass_index)
  // access exception table
  ExceptionTable table(mh());
  int length = table.length();
  // iterate through all entries sequentially
  constantPoolHandle pool(THREAD, mh->constants());
  for (int i = 0; i < length; i ++) {
    //reacquire the table in case a GC happened
    ExceptionTable table(mh());
    int beg_bci = table.start_pc(i);
    int end_bci = table.end_pc(i);
    assert(beg_bci <= end_bci, "inconsistent exception table");
    if (beg_bci <= throw_bci && throw_bci < end_bci) {
      // exception handler bci range covers throw_bci => investigate further
      int handler_bci = table.handler_pc(i);
      int klass_index = table.catch_type_index(i);
      if (klass_index == 0) {
        return handler_bci;
      } else if (ex_klass.is_null()) {
        return handler_bci;
      } else {
        // we know the exception class => get the constraint class
        // this may require loading of the constraint class; if verification
        // fails or some other exception occurs, return handler_bci
        Klass* k = pool->klass_at(klass_index, CHECK_(handler_bci));
        KlassHandle klass = KlassHandle(THREAD, k);
        assert(klass.not_null(), "klass not loaded");
        if (ex_klass->is_subtype_of(klass())) {
          return handler_bci;
        }
      }
    }
  }

  return -1;
}

void Method::mask_for(int bci, InterpreterOopMap* mask) {

  Thread* myThread    = Thread::current();
  methodHandle h_this(myThread, this);
#ifdef ASSERT
  bool has_capability = myThread->is_VM_thread() ||
                        myThread->is_ConcurrentGC_thread() ||
                        myThread->is_GC_task_thread();

  if (!has_capability) {
    if (!VerifyStack && !VerifyLastFrame) {
      // verify stack calls this outside VM thread
      warning("oopmap should only be accessed by the "
              "VM, GC task or CMS threads (or during debugging)");
      InterpreterOopMap local_mask;
      method_holder()->mask_for(h_this, bci, &local_mask);
      local_mask.print();
    }
  }
#endif
  method_holder()->mask_for(h_this, bci, mask);
  return;
}


int Method::bci_from(address bcp) const {
#ifdef ASSERT
  { ResourceMark rm;
  assert(is_native() && bcp == code_base() || contains(bcp) || is_error_reported(),
         err_msg("bcp doesn't belong to this method: bcp: " INTPTR_FORMAT ", method: %s", bcp, name_and_sig_as_C_string()));
  }
#endif
  return bcp - code_base();
}


// Return (int)bcx if it appears to be a valid BCI.
// Return bci_from((address)bcx) if it appears to be a valid BCP.
// Return -1 otherwise.
// Used by profiling code, when invalid data is a possibility.
// The caller is responsible for validating the Method* itself.
int Method::validate_bci_from_bcx(intptr_t bcx) const {
  // keep bci as -1 if not a valid bci
  int bci = -1;
  if (bcx == 0 || (address)bcx == code_base()) {
    // code_size() may return 0 and we allow 0 here
    // the method may be native
    bci = 0;
  } else if (frame::is_bci(bcx)) {
    if (bcx < code_size()) {
      bci = (int)bcx;
    }
  } else if (contains((address)bcx)) {
    bci = (address)bcx - code_base();
  }
  // Assert that if we have dodged any asserts, bci is negative.
  assert(bci == -1 || bci == bci_from(bcp_from(bci)), "sane bci if >=0");
  return bci;
}

address Method::bcp_from(int bci) const {
  assert((is_native() && bci == 0)  || (!is_native() && 0 <= bci && bci < code_size()), err_msg("illegal bci: %d", bci));
  address bcp = code_base() + bci;
  assert(is_native() && bcp == code_base() || contains(bcp), "bcp doesn't belong to this method");
  return bcp;
}


int Method::size(bool is_native) {
  // If native, then include pointers for native_function and signature_handler
  // jianyu: if native, then also include pointers for native_function and signature_handler for enclave
  int extra_bytes = (is_native) ? 2 * sizeof(address*) : 0;
  int extra_words = align_size_up(extra_bytes, BytesPerWord) / BytesPerWord;
  return align_object_size(header_size() + extra_words);
}


Symbol* Method::klass_name() const {
  Klass* k = method_holder();
  assert(k->is_klass(), "must be klass");
  InstanceKlass* ik = (InstanceKlass*) k;
  return ik->name();
}


// Attempt to return method oop to original state.  Clear any pointers
// (to objects outside the shared spaces).  We won't be able to predict
// where they should point in a new JVM.  Further initialize some
// entries now in order allow them to be write protected later.

void Method::remove_unshareable_info() {
  unlink_method();
}

void Method::set_vtable_index(int index) {
  _vtable_index = index;
}

void Method::set_itable_index(int index) {
  _vtable_index = itable_index_max - index;
}



bool Method::was_executed_more_than(int n) {
  D_WARN_Unimplement;
}

#ifndef PRODUCT
void Method::print_invocation_count() {
  if (is_static()) tty->print("static ");
  if (is_final()) tty->print("final ");
  if (is_synchronized()) tty->print("synchronized ");
  if (is_native()) tty->print("native ");
  method_holder()->name()->print_symbol_on(tty);
  tty->print(".");
  name()->print_symbol_on(tty);
  signature()->print_symbol_on(tty);

  if (WizardMode) {
    // dump the size of the byte codes
    tty->print(" {%d}", code_size());
  }
  tty->cr();

  tty->print_cr ("  interpreter_invocation_count: %8d ", interpreter_invocation_count());
  tty->print_cr ("  invocation_counter:           %8d ", invocation_count());
  tty->print_cr ("  backedge_counter:             %8d ", backedge_count());
  if (CountCompiledCalls) {
    tty->print_cr ("  compiled_invocation_count: %8d ", compiled_invocation_count());
  }

}
#endif

// Build a MethodData* object to hold information about this method
// collected in the interpreter.
void Method::build_interpreter_method_data(methodHandle method, TRAPS) {
  D_WARN_Unimplement;
}

MethodCounters* Method::build_method_counters(Method* m, TRAPS) {
  D_WARN_Unimplement;
}

void Method::cleanup_inline_caches() {
  // The current system doesn't use inline caches in the interpreter
  // => nothing to do (keep this method around for future use)
}


int Method::extra_stack_words() {
  // not an inline function, to avoid a header dependency on Interpreter
  return extra_stack_entries() * Interpreter::stackElementSize;
}


void Method::compute_size_of_parameters(Thread *thread) {
  ArgumentSizeComputer asc(signature());
  set_size_of_parameters(asc.size() + (is_static() ? 0 : 1));
}

BasicType Method::result_type() const {
  ResultTypeFinder rtf(signature());
  return rtf.type();
}


bool Method::is_empty_method() const {
  return  code_size() == 1
      && *code_base() == Bytecodes::_return;
}


bool Method::is_vanilla_constructor() const {
  // Returns true if this method is a vanilla constructor, i.e. an "<init>" "()V" method
  // which only calls the superclass vanilla constructor and possibly does stores of
  // zero constants to local fields:
  //
  //   aload_0
  //   invokespecial
  //   indexbyte1
  //   indexbyte2
  //
  // followed by an (optional) sequence of:
  //
  //   aload_0
  //   aconst_null / iconst_0 / fconst_0 / dconst_0
  //   putfield
  //   indexbyte1
  //   indexbyte2
  //
  // followed by:
  //
  //   return

  assert(name() == vmSymbols::object_initializer_name(),    "Should only be called for default constructors");
  assert(signature() == vmSymbols::void_method_signature(), "Should only be called for default constructors");
  int size = code_size();
  // Check if size match
  if (size == 0 || size % 5 != 0) return false;
  address cb = code_base();
  int last = size - 1;
  if (cb[0] != Bytecodes::_aload_0 || cb[1] != Bytecodes::_invokespecial || cb[last] != Bytecodes::_return) {
    // Does not call superclass default constructor
    return false;
  }
  // Check optional sequence
  for (int i = 4; i < last; i += 5) {
    if (cb[i] != Bytecodes::_aload_0) return false;
    if (!Bytecodes::is_zero_const(Bytecodes::cast(cb[i+1]))) return false;
    if (cb[i+2] != Bytecodes::_putfield) return false;
  }
  return true;
}


bool Method::compute_has_loops_flag() {
  D_WARN_Unimplement;
}

bool Method::is_final_method(AccessFlags class_access_flags) const {
  // or "does_not_require_vtable_entry"
  // default method or overpass can occur, is not final (reuses vtable entry)
  // private methods get vtable entries for backward class compatibility.
  if (is_overpass() || is_default_method())  return false;
  return is_final() || class_access_flags.is_final();
}

bool Method::is_final_method() const {
  return is_final_method(method_holder()->access_flags());
}

bool Method::is_default_method() const {
  if (method_holder() != NULL &&
      method_holder()->is_interface() &&
      !is_abstract()) {
    return true;
  } else {
    return false;
  }
}

bool Method::can_be_statically_bound(AccessFlags class_access_flags) const {
  if (is_final_method(class_access_flags))  return true;
#ifdef ASSERT
  ResourceMark rm;
  bool is_nonv = (vtable_index() == nonvirtual_vtable_index);
  if (class_access_flags.is_interface()) {
      assert(is_nonv == is_static(), err_msg("is_nonv=%s", name_and_sig_as_C_string()));
  }
#endif
  assert(valid_vtable_index() || valid_itable_index(), "method must be linked before we ask this question");
  return vtable_index() == nonvirtual_vtable_index;
}

bool Method::can_be_statically_bound() const {
  return can_be_statically_bound(method_holder()->access_flags());
}

bool Method::is_accessor() const {
  if (code_size() != 5) return false;
  if (size_of_parameters() != 1) return false;
  if (java_code_at(0) != Bytecodes::_aload_0 ) return false;
  if (java_code_at(1) != Bytecodes::_getfield) return false;
  if (java_code_at(4) != Bytecodes::_areturn &&
      java_code_at(4) != Bytecodes::_ireturn ) return false;
  return true;
}

bool Method::is_constant_getter() const {
  int last_index = code_size() - 1;
  // Check if the first 1-3 bytecodes are a constant push
  // and the last bytecode is a return.
  return (2 <= code_size() && code_size() <= 4 &&
          Bytecodes::is_const(java_code_at(0)) &&
          Bytecodes::length_for(java_code_at(0)) == last_index &&
          Bytecodes::is_return(java_code_at(last_index)));
}

bool Method::is_initializer() const {
  return is_object_initializer() || is_static_initializer();
}

bool Method::has_valid_initializer_flags() const {
  return (is_static() ||
          method_holder()->major_version() < 51);
}

bool Method::is_static_initializer() const {
  // For classfiles version 51 or greater, ensure that the clinit method is
  // static.  Non-static methods with the name "<clinit>" are not static
  // initializers. (older classfiles exempted for backward compatibility)
  return name() == vmSymbols::class_initializer_name() &&
         has_valid_initializer_flags();
}

bool Method::is_object_initializer() const {
   return name() == vmSymbols::object_initializer_name();
}

objArrayHandle Method::resolved_checked_exceptions_impl(Method* this_oop, TRAPS) {
  D_WARN_Unimplement;
};


int Method::line_number_from_bci(int bci) const {
  D_WARN_Unimplement;
}


bool Method::is_klass_loaded_by_klass_index(int klass_index) const {
  if( constants()->tag_at(klass_index).is_unresolved_klass() ) {
    Thread *thread = Thread::current();
    Symbol* klass_name = constants()->klass_name_at(klass_index);
    Handle loader(thread, method_holder()->class_loader());
    Handle prot  (thread, method_holder()->protection_domain());
    return SystemDictionary::find(klass_name, loader, prot, thread) != NULL;
  } else {
    return true;
  }
}


bool Method::is_klass_loaded(int refinfo_index, bool must_be_resolved) const {
  int klass_index = constants()->klass_ref_index_at(refinfo_index);
  if (must_be_resolved) {
    // Make sure klass is resolved in constantpool.
    if (constants()->tag_at(klass_index).is_unresolved_klass()) return false;
  }
  return is_klass_loaded_by_klass_index(klass_index);
}


void Method::set_native_function(address function, bool post_event_flag) {
  D_WARN_Unimplement;
}

bool Method::has_native_function() const {
  D_WARN_Unimplement;
}


void Method::clear_native_function() {
  D_WARN_Unimplement;
}

address Method::critical_native_function() {
  D_WARN_Unimplement;
}


void Method::set_signature_handler(address handler) {
  address* signature_handler =  signature_handler_addr();
  *signature_handler = handler;
}


void Method::print_made_not_compilable(int comp_level, bool is_osr, bool report, const char* reason) {
  D_WARN_Unimplement;
}

bool Method::is_always_compilable() const {
  // Generated adapters must be compiled
  if (is_method_handle_intrinsic() && is_synthetic()) {
    assert(!is_not_c1_compilable(), "sanity check");
    assert(!is_not_c2_compilable(), "sanity check");
    return true;
  }

  return false;
}

bool Method::is_not_compilable(int comp_level) const {
  if (number_of_breakpoints() > 0)
    return true;
  if (is_sgx_interface(this) || strncmp(name()->as_C_string(), "sgx_hook", 8) == 0) {
    return true;
  }
  if (is_always_compilable())
    return false;
  if (comp_level == CompLevel_any)
    return is_not_c1_compilable() || is_not_c2_compilable();
  if (is_c1_compile(comp_level))
    return is_not_c1_compilable();
  if (is_c2_compile(comp_level))
    return is_not_c2_compilable();
  return false;
}

// call this when compiler finds that this method is not compilable
void Method::set_not_compilable(int comp_level, bool report, const char* reason) {
  D_WARN_Unimplement;
}

bool Method::is_not_osr_compilable(int comp_level) const {
  if (is_not_compilable(comp_level))
    return true;
  if (comp_level == CompLevel_any)
    return is_not_c1_osr_compilable() || is_not_c2_osr_compilable();
  if (is_c1_compile(comp_level))
    return is_not_c1_osr_compilable();
  if (is_c2_compile(comp_level))
    return is_not_c2_osr_compilable();
  return false;
}

void Method::set_not_osr_compilable(int comp_level, bool report, const char* reason) {
  D_WARN_Unimplement;
}

// Revert to using the interpreter and clear out the nmethod
void Method::clear_code(bool acquire_lock /* = true */) {
  MutexLockerEx pl(acquire_lock ? Patching_lock : NULL, Mutex::_no_safepoint_check_flag);
  // this may be NULL if c2i adapters have not been made yet
  // Only should happen at allocate time.
  _from_compiled_entry    = NULL;
  OrderAccess::storestore();
  _from_interpreted_entry = _i2i_entry;
  OrderAccess::storestore();
  _code = NULL;
}

// Called by class data sharing to remove any entry points (which are not shared)
void Method::unlink_method() {
  _code = NULL;
  _i2i_entry = NULL;
  _from_interpreted_entry = NULL;
  if (is_native()) {
    *native_function_addr() = NULL;
    set_signature_handler(NULL);
  }
  NOT_PRODUCT(set_compiled_invocation_count(0);)
  _adapter = NULL;
  _from_compiled_entry = NULL;

  // In case of DumpSharedSpaces, _method_data should always be NULL.
  //
  // During runtime (!DumpSharedSpaces), when we are cleaning a
  // shared class that failed to load, this->link_method() may
  // have already been called (before an exception happened), so
  // this->_method_data may not be NULL.
  assert(!DumpSharedSpaces || _method_data == NULL, "unexpected method data?");

  set_method_data(NULL);
  clear_method_counters();
}

// Called when the method_holder is getting linked. Setup entrypoints so the method
// is ready to be called from interpreter, compiler, and vtables.
void Method::link_method(methodHandle h_method, TRAPS) {
  // If the code cache is full, we may reenter this function for the
  // leftover methods that weren't linked.
  if (_i2i_entry != NULL) return;

  assert(_adapter == NULL, "init'd to NULL" );
  assert( _code == NULL, "nothing compiled yet" );

  // Setup interpreter entrypoint
  assert(this == h_method(), "wrong h_method()" );
  address entry = Interpreter::entry_for_method(h_method);
  _method_kind = Interpreter::method_kind(h_method);
  assert(entry != NULL, "interpreter entry must be non-null");
  // Sets both _i2i_entry and _from_interpreted_entry
  set_interpreter_entry(entry);
  // int sgx_flags = is_sgx_interface(this);
  // if ((sgx_flags & sgx_ecall_flag) == sgx_ecall_flag || strncmp(name()->as_C_string(), "sgx_hook", 8) == 0) {
  //   {
  //     _i2i_entry = get_non_null_point();
  //     _from_interpreted_entry = _i2i_entry;
  //   }
  //   if ((sgx_flags & sgx_func_flag) == sgx_func_flag) {
  //     AccessFlags f = this->access_flags();
  //     f.set_flags(f.get_flags() | JVM_SGX_FUNC);
  //     this->set_access_flags(f);
  //   }
  // } else if ((sgx_flags & sgx_ocall_flag) == sgx_ocall_flag || strncmp(name()->as_C_string(), "sgx_unhook", 10) == 0) {
  //     this->_method_kind = AbstractInterpreter::sgx_ocall_entry;
  // }

  // Don't overwrite already registered native entries.
  // if (is_native() && !has_native_function()) {
  //   set_native_function(
  //     SharedRuntime::native_method_throw_unsatisfied_link_error_entry(),
  //     !native_bind_event_is_interesting);
  // }

  // Setup compiler entrypoint.  This is made eagerly, so we do not need
  // special handling of vtables.  An alternative is to make adapters more
  // lazily by calling make_adapter() from from_compiled_entry() for the
  // normal calls.  For vtable calls life gets more complicated.  When a
  // call-site goes mega-morphic we need adapters in all methods which can be
  // called from the vtable.  We need adapters on such methods that get loaded
  // later.  Ditto for mega-morphic itable calls.  If this proves to be a
  // problem we'll make these lazily later.
  // (void) make_adapters(h_method, CHECK);

  // ONLY USE the h_method now as make_adapter may have blocked

}

address Method::make_adapters(methodHandle mh, TRAPS) {
  D_WARN_Unimplement;
}

void Method::restore_unshareable_info(TRAPS) {
  // Since restore_unshareable_info can be called more than once for a method, don't
  // redo any work.   If this field is restored, there is nothing to do.
  if (_from_compiled_entry == NULL) {
    // restore method's vtable by calling a virtual function
    restore_vtable();

    methodHandle mh(THREAD, this);
    link_method(mh, CHECK);
  }
}


// The verified_code_entry() must be called when a invoke is resolved
// on this method.

// It returns the compiled code entry point, after asserting not null.
// This function is called after potential safepoints so that nmethod
// or adapter that it points to is still live and valid.
// This function must not hit a safepoint!
address Method::verified_code_entry() {
  debug_only(No_Safepoint_Verifier nsv;)
  assert(_from_compiled_entry != NULL, "must be set");
  return _from_compiled_entry;
}

// Check that if an nmethod ref exists, it has a backlink to this or no backlink at all
// (could be racing a deopt).
// Not inline to avoid circular ref.
bool Method::check_code() const {
  D_WARN_Unimplement;
}

// Install compiled code.  Instantly it can execute.
void Method::set_code(methodHandle mh, nmethod *code) {
  D_WARN_Unimplement;
}


bool Method::is_overridden_in(Klass* k) const {
  InstanceKlass* ik = InstanceKlass::cast(k);

  if (ik->is_interface()) return false;

  // If method is an interface, we skip it - except if it
  // is a miranda method
  if (method_holder()->is_interface()) {
    // Check that method is not a miranda method
    if (ik->lookup_method(name(), signature()) == NULL) {
      // No implementation exist - so miranda method
      return false;
    }
    return true;
  }

  assert(ik->is_subclass_of(method_holder()), "should be subklass");
  assert(ik->vtable() != NULL, "vtable should exist");
  if (!has_vtable_index()) {
    return false;
  } else {
    Method* vt_m = ik->method_at_vtable(vtable_index());
    return vt_m != this;
  }
}


// give advice about whether this Method* should be cached or not
bool Method::should_not_be_cached() const {
  if (is_old()) {
    // This method has been redefined. It is either EMCP or obsolete
    // and we don't want to cache it because that would pin the method
    // down and prevent it from being collectible if and when it
    // finishes executing.
    return true;
  }

  // caching this method should be just fine
  return false;
}


/**
 *  Returns true if this is one of the specially treated methods for
 *  security related stack walks (like Reflection.getCallerClass).
 */
bool Method::is_ignored_by_security_stack_walk() const {
  D_WARN_Unimplement;
}


// Constant pool structure for invoke methods:
enum {
  _imcp_invoke_name = 1,        // utf8: 'invokeExact', etc.
  _imcp_invoke_signature,       // utf8: (variable Symbol*)
  _imcp_limit
};

// Test if this method is an MH adapter frame generated by Java code.
// Cf. java/lang/invoke/InvokerBytecodeGenerator
bool Method::is_compiled_lambda_form() const {
  return intrinsic_id() == vmIntrinsics::_compiledLambdaForm;
}

// Test if this method is an internal MH primitive method.
bool Method::is_method_handle_intrinsic() const {
  vmIntrinsics::ID iid = intrinsic_id();
  return (MethodHandles::is_signature_polymorphic(iid) &&
          MethodHandles::is_signature_polymorphic_intrinsic(iid));
}

bool Method::has_member_arg() const {
  vmIntrinsics::ID iid = intrinsic_id();
  return (MethodHandles::is_signature_polymorphic(iid) &&
          MethodHandles::has_member_arg(iid));
}

// Make an instance of a signature-polymorphic internal MH primitive.
methodHandle Method::make_method_handle_intrinsic(vmIntrinsics::ID iid,
                                                         Symbol* signature,
                                                         TRAPS) {
  D_WARN_Unimplement;
}

Klass* Method::check_non_bcp_klass(Klass* klass) {
  if (klass != NULL && klass->class_loader() != NULL) {
    if (klass->oop_is_objArray())
      klass = ObjArrayKlass::cast(klass)->bottom_klass();
    return klass;
  }
  return NULL;
}


methodHandle Method::clone_with_new_data(methodHandle m, u_char* new_code, int new_code_length,
                                                u_char* new_compressed_linenumber_table, int new_compressed_linenumber_size, TRAPS) {
  D_WARN_Unimplement;
}

vmSymbols::SID Method::klass_id_for_intrinsics(Klass* holder) {
  // if loader is not the default loader (i.e., != NULL), we can't know the intrinsics
  // because we are not loading from core libraries
  // exception: the AES intrinsics come from lib/ext/sunjce_provider.jar
  // which does not use the class default class loader so we check for its loader here
  InstanceKlass* ik = InstanceKlass::cast(holder);
  if ((ik->class_loader() != NULL) && !SystemDictionary::is_ext_class_loader(ik->class_loader())) {
    return vmSymbols::NO_SID;   // regardless of name, no intrinsics here
  }

  // see if the klass name is well-known:
  Symbol* klass_name = ik->name();
  return vmSymbols::find_sid(klass_name);
}

void Method::init_intrinsic_id() {
  assert(_intrinsic_id == vmIntrinsics::_none, "do this just once");
  const uintptr_t max_id_uint = right_n_bits((int)(sizeof(_intrinsic_id) * BitsPerByte));
  assert((uintptr_t)vmIntrinsics::ID_LIMIT <= max_id_uint, "else fix size");
  assert(intrinsic_id_size_in_bytes() == sizeof(_intrinsic_id), "");

  // the klass name is well-known:
  vmSymbols::SID klass_id = klass_id_for_intrinsics(method_holder());
  assert(klass_id != vmSymbols::NO_SID, "caller responsibility");

  // ditto for method and signature:
  vmSymbols::SID  name_id = vmSymbols::find_sid(name());
  if (klass_id != vmSymbols::VM_SYMBOL_ENUM_NAME(java_lang_invoke_MethodHandle)
      && name_id == vmSymbols::NO_SID)
    return;
  vmSymbols::SID   sig_id = vmSymbols::find_sid(signature());
  if (klass_id != vmSymbols::VM_SYMBOL_ENUM_NAME(java_lang_invoke_MethodHandle)
      && sig_id == vmSymbols::NO_SID)  return;
  jshort flags = access_flags().as_short();

  vmIntrinsics::ID id = vmIntrinsics::find_id(klass_id, name_id, sig_id, flags);
  if (id != vmIntrinsics::_none) {
    set_intrinsic_id(id);
    return;
  }

  // A few slightly irregular cases:
  switch (klass_id) {
  case vmSymbols::VM_SYMBOL_ENUM_NAME(java_lang_StrictMath):
    // Second chance: check in regular Math.
    switch (name_id) {
    case vmSymbols::VM_SYMBOL_ENUM_NAME(min_name):
    case vmSymbols::VM_SYMBOL_ENUM_NAME(max_name):
    case vmSymbols::VM_SYMBOL_ENUM_NAME(sqrt_name):
      // pretend it is the corresponding method in the non-strict class:
      klass_id = vmSymbols::VM_SYMBOL_ENUM_NAME(java_lang_Math);
      id = vmIntrinsics::find_id(klass_id, name_id, sig_id, flags);
      break;
    }
    break;

  // Signature-polymorphic methods: MethodHandle.invoke*, InvokeDynamic.*.
  case vmSymbols::VM_SYMBOL_ENUM_NAME(java_lang_invoke_MethodHandle):
    if (!is_native())  break;
    id = MethodHandles::signature_polymorphic_name_id(method_holder(), name());
    if (is_static() != MethodHandles::is_signature_polymorphic_static(id))
      id = vmIntrinsics::_none;
    break;
  }

  if (id != vmIntrinsics::_none) {
    // Set up its iid.  It is an alias method.
    set_intrinsic_id(id);
    return;
  }
}

// These two methods are static since a GC may move the Method
bool Method::load_signature_classes(methodHandle m, TRAPS) {
  D_WARN_Unimplement;
}

bool Method::has_unloaded_classes_in_signature(methodHandle m, TRAPS) {
  Handle class_loader(THREAD, m->method_holder()->class_loader());
  Handle protection_domain(THREAD, m->method_holder()->protection_domain());
  ResourceMark rm(THREAD);
  Symbol*  signature = m->signature();
  for(SignatureStream ss(signature); !ss.is_done(); ss.next()) {
    if (ss.type() == T_OBJECT) {
      Symbol* name = ss.as_symbol_or_null();
      if (name == NULL) return true;
      Klass* klass = SystemDictionary::find(name, class_loader, protection_domain, THREAD);
      if (klass == NULL) return true;
    }
  }
  return false;
}

// Exposed so field engineers can debug VM
void Method::print_short_name(outputStream* st) {
  D_WARN_Unimplement;
}

// Comparer for sorting an object array containing
// Method*s.
static int method_comparator(Method* a, Method* b) {
  return a->name()->fast_compare(b->name());
}

// This is only done during class loading, so it is OK to assume method_idnum matches the methods() array
// default_methods also uses this without the ordering for fast find_method
void Method::sort_methods(Array<Method*>* methods, bool idempotent, bool set_idnums) {
  int length = methods->length();
  if (length > 1) {
    {
      // No_Safepoint_Verifier nsv;
      QuickSort::sort<Method*>(methods->data(), length, method_comparator, idempotent);
    }
    // Reset method ordering
    if (set_idnums) {
      for (int i = 0; i < length; i++) {
        Method* m = methods->at(i);
        m->set_method_idnum(i);
        m->set_orig_method_idnum(i);
      }
    }
  }
}

//-----------------------------------------------------------------------------------
// Non-product code unless JVM/TI needs it

#if !defined(PRODUCT) || INCLUDE_JVMTI
class SignatureTypePrinter : public SignatureTypeNames {
 private:
  outputStream* _st;
  bool _use_separator;

  void type_name(const char* name) {
    if (_use_separator) _st->print(", ");
    _st->print("%s", name);
    _use_separator = true;
  }

 public:
  SignatureTypePrinter(Symbol* signature, outputStream* st) : SignatureTypeNames(signature) {
    _st = st;
    _use_separator = false;
  }

  void print_parameters()              { _use_separator = false; iterate_parameters(); }
  void print_returntype()              { _use_separator = false; iterate_returntype(); }
};


void Method::print_name(outputStream* st) {
  Thread *thread = Thread::current();
  ResourceMark rm(thread);
  SignatureTypePrinter sig(signature(), st);
  st->print("%s ", is_static() ? "static" : "virtual");
  sig.print_returntype();
  st->print(" %s.", method_holder()->internal_name());
  name()->print_symbol_on(st);
  st->print("(");
  sig.print_parameters();
  st->print(")");
}
#endif // !PRODUCT || INCLUDE_JVMTI


//-----------------------------------------------------------------------------------
// Non-product code

#ifndef PRODUCT
void Method::print_codes_on(outputStream* st) const {
  print_codes_on(0, code_size(), st);
}

void Method::print_codes_on(int from, int to, outputStream* st) const {
  D_WARN_Unimplement;
}
#endif // not PRODUCT


// Simple compression of line number tables. We use a regular compressed stream, except that we compress deltas
// between (bci,line) pairs since they are smaller. If (bci delta, line delta) fits in (5-bit unsigned, 3-bit unsigned)
// we save it as one byte, otherwise we write a 0xFF escape character and use regular compression. 0x0 is used
// as end-of-stream terminator.

void CompressedLineNumberWriteStream::write_pair_regular(int bci_delta, int line_delta) {
    // bci and line number does not compress into single byte.
    // Write out escape character and use regular compression for bci and line number.
    write_byte((jubyte)0xFF);
    write_signed_int(bci_delta);
    write_signed_int(line_delta);
}

// See comment in method.hpp which explains why this exists.
#if defined(_M_AMD64) && _MSC_VER >= 1400
#pragma optimize("", off)
void CompressedLineNumberWriteStream::write_pair(int bci, int line) {
    write_pair_inline(bci, line);
}
#pragma optimize("", on)
#endif

CompressedLineNumberReadStream::CompressedLineNumberReadStream(u_char* buffer) : CompressedReadStream(buffer) {
    _bci = 0;
    _line = 0;
};


bool CompressedLineNumberReadStream::read_pair() {
    jubyte next = read_byte();
    // Check for terminator
    if (next == 0) return false;
    if (next == 0xFF) {
        // Escape character, regular compression used
        _bci += read_signed_int();
        _line += read_signed_int();
    }
    else {
        // Single byte compression used
        _bci += next >> 3;
        _line += next & 0x7;
    }
    return true;
}


// See comment in method.hpp which explains why this exists.
#if defined(_M_AMD64) && _MSC_VER >= 1400
#pragma optimize("", off)
void CompressedLineNumberWriteStream::write_pair(int bci, int line) {
  write_pair_inline(bci, line);
}
#pragma optimize("", on)
#endif


Bytecodes::Code Method::orig_bytecode_at(int bci) const {
  BreakpointInfo* bp = method_holder()->breakpoints();
  for (; bp != NULL; bp = bp->next()) {
    if (bp->match(this, bci)) {
      return bp->orig_bytecode();
    }
  }
  {
    ResourceMark rm;
    fatal(err_msg("no original bytecode found in %s at bci %d", name_and_sig_as_C_string(), bci));
  }
  return Bytecodes::_shouldnotreachhere;
}

void Method::set_orig_bytecode_at(int bci, Bytecodes::Code code) {
  assert(code != Bytecodes::_breakpoint, "cannot patch breakpoints this way");
  BreakpointInfo* bp = method_holder()->breakpoints();
  for (; bp != NULL; bp = bp->next()) {
    if (bp->match(this, bci)) {
      bp->set_orig_bytecode(code);
      // and continue, in case there is more than one
    }
  }
}

void Method::set_breakpoint(int bci) {
  InstanceKlass* ik = method_holder();
  BreakpointInfo *bp = new BreakpointInfo(this, bci);
  bp->set_next(ik->breakpoints());
  ik->set_breakpoints(bp);
  // do this last:
  bp->set(this);
}

static void clear_matches(Method* m, int bci) {
  InstanceKlass* ik = m->method_holder();
  BreakpointInfo* prev_bp = NULL;
  BreakpointInfo* next_bp;
  for (BreakpointInfo* bp = ik->breakpoints(); bp != NULL; bp = next_bp) {
    next_bp = bp->next();
    // bci value of -1 is used to delete all breakpoints in method m (ex: clear_all_breakpoint).
    if (bci >= 0 ? bp->match(m, bci) : bp->match(m)) {
      // do this first:
      bp->clear(m);
      // unhook it
      if (prev_bp != NULL)
        prev_bp->set_next(next_bp);
      else
        ik->set_breakpoints(next_bp);
      delete bp;
      // When class is redefined JVMTI sets breakpoint in all versions of EMCP methods
      // at same location. So we have multiple matching (method_index and bci)
      // BreakpointInfo nodes in BreakpointInfo list. We should just delete one
      // breakpoint for clear_breakpoint request and keep all other method versions
      // BreakpointInfo for future clear_breakpoint request.
      // bcivalue of -1 is used to clear all breakpoints (see clear_all_breakpoints)
      // which is being called when class is unloaded. We delete all the Breakpoint
      // information for all versions of method. We may not correctly restore the original
      // bytecode in all method versions, but that is ok. Because the class is being unloaded
      // so these methods won't be used anymore.
      if (bci >= 0) {
        break;
      }
    } else {
      // This one is a keeper.
      prev_bp = bp;
    }
  }
}

void Method::clear_breakpoint(int bci) {
  assert(bci >= 0, "");
  clear_matches(this, bci);
}

void Method::clear_all_breakpoints() {
  clear_matches(this, -1);
}


int Method::invocation_count() {
  D_WARN_Unimplement;
}

int Method::backedge_count() {
  D_WARN_Unimplement;
}

int Method::highest_comp_level() const {
  D_WARN_Unimplement;
}

int Method::highest_osr_comp_level() const {
  D_WARN_Unimplement;
}

void Method::set_highest_comp_level(int level) {
  D_WARN_Unimplement;
}

void Method::set_highest_osr_comp_level(int level) {
  D_WARN_Unimplement;
}

BreakpointInfo::BreakpointInfo(Method* m, int bci) {
  _bci = bci;
  _name_index = m->name_index();
  _signature_index = m->signature_index();
  _orig_bytecode = (Bytecodes::Code) *m->bcp_from(_bci);
  if (_orig_bytecode == Bytecodes::_breakpoint)
    _orig_bytecode = m->orig_bytecode_at(_bci);
  _next = NULL;
}

void BreakpointInfo::set(Method* method) {
#ifdef ASSERT
  {
    Bytecodes::Code code = (Bytecodes::Code) *method->bcp_from(_bci);
    if (code == Bytecodes::_breakpoint)
      code = method->orig_bytecode_at(_bci);
    assert(orig_bytecode() == code, "original bytecode must be the same");
  }
#endif
  Thread *thread = Thread::current();
  *method->bcp_from(_bci) = Bytecodes::_breakpoint;
  method->incr_number_of_breakpoints(thread);
  SystemDictionary::notice_modification();
  {
    // Deoptimize all dependents on this method
    HandleMark hm(thread);
    methodHandle mh(thread, method);
    Universe::flush_dependents_on_method(mh);
  }
}

void BreakpointInfo::clear(Method* method) {
  *method->bcp_from(_bci) = orig_bytecode();
  assert(method->number_of_breakpoints() > 0, "must not go negative");
  method->decr_number_of_breakpoints(Thread::current());
}

// jmethodID handling

// This is a block allocating object, sort of like JNIHandleBlock, only a
// lot simpler.  There aren't many of these, they aren't long, they are rarely
// deleted and so we can do some suboptimal things.
// It's allocated on the CHeap because once we allocate a jmethodID, we can
// never get rid of it.
// It would be nice to be able to parameterize the number of methods for
// the null_class_loader but then we'd have to turn this and ClassLoaderData
// into templates.

// I feel like this brain dead class should exist somewhere in the STL

class JNIMethodBlock : public CHeapObj<mtClass> {
  enum { number_of_methods = 8 };

  Method*         _methods[0];
  int             _top;
  JNIMethodBlock* _next;
 public:
  static Method* const _free_method;

  JNIMethodBlock() : _next(NULL), _top(0) {
    for (int i = 0; i< number_of_methods; i++) _methods[i] = _free_method;
  }

  Method** add_method(Method* m) {
    if (_top < number_of_methods) {
      // top points to the next free entry.
      int i = _top;
      _methods[i] = m;
      _top++;
      return &_methods[i];
    } else if (_top == number_of_methods) {
      // if the next free entry ran off the block see if there's a free entry
      for (int i = 0; i< number_of_methods; i++) {
        if (_methods[i] == _free_method) {
          _methods[i] = m;
          return &_methods[i];
        }
      }
      // Only check each block once for frees.  They're very unlikely.
      // Increment top past the end of the block.
      _top++;
    }
    // need to allocate a next block.
    if (_next == NULL) {
      _next = new JNIMethodBlock();
    }
    return _next->add_method(m);
  }

  bool contains(Method** m) {
    for (JNIMethodBlock* b = this; b != NULL; b = b->_next) {
      for (int i = 0; i< number_of_methods; i++) {
        if (&(b->_methods[i]) == m) {
          return true;
        }
      }
    }
    return false;  // not found
  }

  // Doesn't really destroy it, just marks it as free so it can be reused.
  void destroy_method(Method** m) {
#ifdef ASSERT
    assert(contains(m), "should be a methodID");
#endif // ASSERT
    *m = _free_method;
  }
  void clear_method(Method* m) {
    for (JNIMethodBlock* b = this; b != NULL; b = b->_next) {
      for (int i = 0; i < number_of_methods; i++) {
        if (b->_methods[i] == m) {
          b->_methods[i] = NULL;
          return;
        }
      }
    }
    // not found
  }

  // During class unloading the methods are cleared, which is different
  // than freed.
  void clear_all_methods() {
    for (JNIMethodBlock* b = this; b != NULL; b = b->_next) {
      for (int i = 0; i< number_of_methods; i++) {
        b->_methods[i] = NULL;
      }
    }
  }
#ifndef PRODUCT
  int count_methods() {
    // count all allocated methods
    int count = 0;
    for (JNIMethodBlock* b = this; b != NULL; b = b->_next) {
      for (int i = 0; i< number_of_methods; i++) {
        if (b->_methods[i] != _free_method) count++;
      }
    }
    return count;
  }
#endif // PRODUCT
};

// Something that can't be mistaken for an address or a markOop
Method* const JNIMethodBlock::_free_method = (Method*)55;

// Add a method id to the jmethod_ids
jmethodID Method::make_jmethod_id(ClassLoaderData* loader_data, Method* m) {
  D_WARN_Unimplement;
}

// Mark a jmethodID as free.  This is called when there is a data race in
// InstanceKlass while creating the jmethodID cache.
void Method::destroy_jmethod_id(ClassLoaderData* loader_data, jmethodID m) {
  ClassLoaderData* cld = loader_data;
  Method** ptr = (Method**)m;
  assert(cld->jmethod_ids() != NULL, "should have method handles");
  cld->jmethod_ids()->destroy_method(ptr);
}

void Method::change_method_associated_with_jmethod_id(jmethodID jmid, Method* new_method) {
  // Can't assert the method_holder is the same because the new method has the
  // scratch method holder.
  assert(resolve_jmethod_id(jmid)->method_holder()->class_loader()
           == new_method->method_holder()->class_loader(),
         "changing to a different class loader");
  // Just change the method in place, jmethodID pointer doesn't change.
  *((Method**)jmid) = new_method;
}

bool Method::is_method_id(jmethodID mid) {
  Method* m = resolve_jmethod_id(mid);
  if (m == NULL) {
    return false;
  }
  InstanceKlass* ik = m->method_holder();
  if (ik == NULL) {
    return false;
  }
  ClassLoaderData* cld = ik->class_loader_data();
  if (cld->jmethod_ids() == NULL) return false;
  return (cld->jmethod_ids()->contains((Method**)mid));
}

Method* Method::checked_resolve_jmethod_id(jmethodID mid) {
  if (mid == NULL) return NULL;
  if (!Method::is_method_id(mid)) {
    return NULL;
  }
  Method* o = resolve_jmethod_id(mid);
  if (o == NULL || o == JNIMethodBlock::_free_method || !((Metadata*)o)->is_method()) {
    return NULL;
  }
  return o;
};

void Method::set_on_stack(const bool value) {
  D_WARN_Unimplement;
}

void Method::clear_jmethod_id(ClassLoaderData* loader_data) {
  loader_data->jmethod_ids()->clear_method(this);
}

// Called when the class loader is unloaded to make all methods weak.
void Method::clear_jmethod_ids(ClassLoaderData* loader_data) {
  loader_data->jmethod_ids()->clear_all_methods();
}

bool Method::has_method_vptr(const void* ptr) {
  Method m;
  // This assumes that the vtbl pointer is the first word of a C++ object.
  // This assumption is also in universe.cpp patch_klass_vtble
  void* vtbl2 = dereference_vptr((const void*)&m);
  void* this_vtbl = dereference_vptr(ptr);
  return vtbl2 == this_vtbl;
}

// Check that this pointer is valid by checking that the vtbl pointer matches
bool Method::is_valid_method() const {
  if (this == NULL) {
    return false;
  } else if (!is_metaspace_object()) {
    return false;
  } else {
    return has_method_vptr((const void*)this);
  }
}

#ifndef PRODUCT
void Method::print_jmethod_ids(ClassLoaderData* loader_data, outputStream* out) {
  out->print_cr("jni_method_id count = %d", loader_data->jmethod_ids()->count_methods());
}
#endif // PRODUCT


// Printing

#ifndef PRODUCT

void Method::print_on(outputStream* st) const {
  D_WARN_Unimplement;
}

#endif //PRODUCT

void Method::print_value_on(outputStream* st) const {
  assert(is_method(), "must be method");
  st->print("%s", internal_name());
  print_address_on(st);
  st->print(" ");
  name()->print_value_on(st);
  st->print(" ");
  signature()->print_value_on(st);
  st->print(" in ");
  method_holder()->print_value_on(st);
  if (WizardMode) st->print("#%d", _vtable_index);
  if (WizardMode) st->print("[%d,%d]", size_of_parameters(), max_locals());
  if (WizardMode && code() != NULL) st->print(" ((nmethod*)%p)", code());
}

#if INCLUDE_SERVICES
// Size Statistics
void Method::collect_statistics(KlassSizeStats *sz) const {
  D_WARN_Unimplement;
}
#endif // INCLUDE_SERVICES

// Verification

void Method::verify_on(outputStream* st) {
  guarantee(is_method(), "object must be method");
  guarantee(constants()->is_constantPool(), "should be constant pool");
  guarantee(constMethod()->is_constMethod(), "should be ConstMethod*");
  MethodData* md = method_data();
  guarantee(md == NULL ||
      md->is_methodData(), "should be method data");
}
