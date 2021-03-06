//
// Created by jianyu on 11/11/18.
//

#include <precompiled.hpp>
#include <classfile/symbolTable.hpp>
#include "enclave/EnclaveDebug.h"
#include "oops/markOop.hpp"
#include <interpreter/bytecode.hpp>
#include <code/codeBlob.hpp>
#include <runtime/sharedRuntime.hpp>
#include <nativeInst_aarch64.hpp>
#include <runtime/fieldType.hpp>
#include <enclave/EnclaveNative.h>
#include "interpreter/interpreter.hpp"
#include "c0/bytecodeStream.hpp"
#include "c0/c0_CodeStubs.hpp"
#include "c0/MetadataAccessor.hpp"
#include "c0/c0_Runtime.hpp"
#include "c0/c0_NormalCompileTask.hpp"
#include "c0_Disassembler_aarch64.hpp"
//add more include
#include "interpreter/templateTable.hpp"
#include "oops/method.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "interpreter/bytecodes.hpp"
//end add more include
#include <interpreter/interpreterRuntime.hpp>
#define __ _masm->
#define transition(from, to)

// 1. If we expect vtos, push rax into stacks
// 2. If we expect non-tos, pop to rax
// 3. If we have (vtos, vtos), the tos is decided in the generator

extern int is_sgx_interface(const Method* m);

extern Klass* resolve_field_return_klass(methodHandle caller, int bci, TRAPS);

#define init_klass(klass) if (!InstanceKlass::cast(klass)->is_initialized()) { InstanceKlass::cast(klass)->initialize(JavaThread::current()); }

Address at_bcp(int offset) {
  assert(_desc->uses_bcp(), "inconsistent uses_bcp information");
  return Address(rbcp, offset);
}

static void error_backtrace(JavaThread* thread) {
  JavaThread* THREAD = thread;
  frame f = thread->last_frame();
  Method* method;
  RegisterMap reg_map(thread, false);
  while(within_enclave(f.unextended_sp())) {
    method = f.interpreter_frame_method();
    printf("frame %s:%s:%d\n", method->klass_name()->as_C_string(), method->name()->as_C_string(), f.interpreter_frame_bcx());
    f = f.sender(&reg_map);
  }
}

// rlocals points to first parameter
static inline Address iaddress(int n) {
    return Address(rlocals, Interpreter::local_offset_in_bytes(n));
}

static inline Address laddress(int n) {
    return iaddress(n + 1);
}

static inline Address faddress(int n) {
    return iaddress(n);
}

static inline Address daddress(int n) {
    return laddress(n);
}

static inline Address aaddress(int n) {
    return iaddress(n);
}

static inline Address iaddress(Register r) {
    return Address(rlocals, r, Address::lsl(3));
}

static inline Address laddress(Register r, Register scratch,
                               InterpreterMacroAssembler* _masm) {
    __ lea(scratch, Address(rlocals, r, Address::lsl(3)));
    return Address(scratch, Interpreter::local_offset_in_bytes(1));
}

static inline Address faddress(Register r) {
    return iaddress(r);
}

static inline Address daddress(Register r, Register scratch,
                               InterpreterMacroAssembler* _masm) {
    return laddress(r, scratch, _masm);
}

static inline Address aaddress(Register r) {
    return iaddress(r);
}

static inline Address at_rsp() {
    return Address(esp, 0);
}

// At top of Java expression stack which may be different than esp().  It
// isn't for category 1 objects.
static inline Address at_tos   () {
    return Address(esp,  Interpreter::expr_offset_in_bytes(0));
}

static inline Address at_tos_p1() {
    return Address(esp,  Interpreter::expr_offset_in_bytes(1));
}

static inline Address at_tos_p2() {
    return Address(esp,  Interpreter::expr_offset_in_bytes(2));
}

static inline Address at_tos_p3() {
    return Address(esp,  Interpreter::expr_offset_in_bytes(3));
}

static inline Address at_tos_p4() {
    return Address(esp,  Interpreter::expr_offset_in_bytes(4));
}

static inline Address at_tos_p5() {
    return Address(esp,  Interpreter::expr_offset_in_bytes(5));
}

// Condition conversion
static Assembler::Condition j_not(NormalCompileTask::Condition cc) {
    switch (cc) {
        case NormalCompileTask::equal        : return Assembler::NE;
        case NormalCompileTask::notEqual    : return Assembler::EQ;
        case NormalCompileTask::less         : return Assembler::GE;
        case NormalCompileTask::lessEqual   : return Assembler::GT;
        case NormalCompileTask::greater      : return Assembler::LE;
        case NormalCompileTask::greaterEqual: return Assembler::LT;
    }
    ShouldNotReachHere();
    return Assembler::EQ;
}

// At top of Java expression stack which may be different than esp().  It
// isn't for category 1 objects.
//static inline Address at_tos   () {
//    return Address(rsp,  Interpreter::expr_offset_in_bytes(0),		vtos, itos);
//}
//
//static inline Address at_tos_p1() {
//    return Address(rsp,  Interpreter::expr_offset_in_bytes(1),		vtos, itos);
//}
//
//static inline Address at_tos_p2() {
//    return Address(rsp,  Interpreter::expr_offset_in_bytes(2),		vtos, itos);
//}
//
//static inline Address at_tos_p3() {
//    return Address(rsp,  Interpreter::expr_offset_in_bytes(3),		vtos, itos);
//}


// Note: 'double' and 'long long' have 32-bits alignment on x86.


static jlong* double_quadword(jlong *adr, jlong lo, jlong hi) {
    // Use the expression (adr)&(~0xF) to provide 128-bits aligned address
    // of 128-bits operands for SSE instructions.
    jlong *operand = (jlong*)(((intptr_t)adr)&((intptr_t)(~0xF)));
    // Store the value to a 128-bits operand.
    operand[0] = lo;
    operand[1] = hi;
    return operand;
}

// Buffer for 128-bits masks used by SSE instructions.
static jlong float_signflip_pool[2*2];
static jlong double_signflip_pool[2*2];

// Miscelaneous helper routines
// Store an oop (or NULL) at the Address described by obj.
// If val == noreg this means store a NULL

void NormalCompileTask::nop() {
    transition(vtos, vtos);
}
void NormalCompileTask::aconst_null() { 
    transition(vtos, atos);
  __ mov(r0, 0); 
}
void NormalCompileTask::iconst(int value) {
  transition(vtos, itos);
  __ mov(r0, value);
}
void NormalCompileTask::lconst(int value) {
  __ mov(r0, value);
}
void NormalCompileTask::fconst(int value) {
  transition(vtos, ftos);
  switch (value) {
  case 0:
    __ fmovs(v0, zr);
    break;
  case 1:
    __ fmovs(v0, 1.0);
    break;
  case 2:
    __ fmovs(v0, 2.0);
    break;
  default:
    ShouldNotReachHere();
    break;
  }
}
void NormalCompileTask::dconst(int value) {
  transition(vtos, dtos);
  switch (value) {
  case 0:
    __ fmovd(v0, zr);
    break;
  case 1:
    __ fmovd(v0, 1.0);
    break;
  case 2:
    __ fmovd(v0, 2.0);
    break;
  default:
    ShouldNotReachHere();
    break;
  }
}
void NormalCompileTask::bipush() {
    transition(vtos, itos);
    __ mov(r0, bs->get_index_signed_1());
}
void NormalCompileTask::sipush() {
    transition(vtos, itos);
    uint16_t uvalue = bs->get_index_u2();
    int16_t* signed_value_pointer = (int16_t*)&uvalue;
    __ mov(r0, *signed_value_pointer);
}

void NormalCompileTask::ldc(bool wide) {
  int idx = 0;
  int cache_idx = -1;
  if (wide) {
      idx = bs->get_index_u2();
  } else {
      idx = bs->get_index_u1();
  }

  if (tos != vtos) {
      __ push(tos);
  }

  if (Bytecodes::uses_cp_cache(bs->raw_code())) {
      cache_idx = idx;
      idx = method->constants()->object_to_cp_index(idx);
  }

  constantTag tag = method->constants()->tag_at(idx);
  if (tag.is_unresolved_klass() || tag.is_unresolved_klass_in_error() || tag.is_klass()) {
    // call ldc
    oop resolve_constant = NULL;
    if (Bytecodes::uses_cp_cache(bs->raw_code())) {
        resolve_constant = method->constants()->resolve_cached_constant_at(-cache_idx, JavaThread::current());
    }
    if (resolve_constant == NULL) {
        if (will_run) {
            Bytecode_loadconstant bytecodeLoadconstant(methodHandle(method), bs->bci());
            resolve_constant = bytecodeLoadconstant.resolve_constant(JavaThread::current());
            __ movptr(r0, (intptr_t)resolve_constant);
        } else {
            PatchingStub *patchingStub = new PatchingStub(_masm, PatchingStub::load_mirror_id, bs->bci());
            __ movptr(r0, (long)NULL_WORD);
            patchingStub->install();
            append_stub(patchingStub);
        }
    } else {
        __ movptr(r0, (intptr_t)resolve_constant);
    }
    tos = atos;
} else if (tag.is_string()) {
    // call ldc
    oop resolve_constant = NULL;
    oop enclave_constant = NULL;
    // TODO: move it to somewhere else
    if (!((InstanceKlass*)SystemDictionary::String_klass())->is_initialized()) {
      SystemDictionary::String_klass()->initialize(JavaThread::current());
    }
    if (Bytecodes::uses_cp_cache(bs->raw_code())) {
        resolve_constant = method->constants()->resolve_cached_constant_at(-cache_idx, JavaThread::current());
    }
    if (resolve_constant == NULL) {
        if (will_run) {
            Bytecode_loadconstant bytecodeLoadconstant(methodHandle(method), bs->bci());
            resolve_constant = bytecodeLoadconstant.resolve_constant(JavaThread::current());
            enclave_constant = StringTable::intern(resolve_constant, JavaThread::current());
            __ movptr(r0, (intptr_t)enclave_constant);
        } else {
            PatchingStub *patchingStub = new PatchingStub(_masm, PatchingStub::load_mirror_id, bs->bci());
            __ movptr(r0, NULL_WORD);
            patchingStub->install();
            append_stub(patchingStub);
        }
    } else {
        enclave_constant = StringTable::intern(resolve_constant, JavaThread::current());
        __ movptr(r0, (intptr_t)enclave_constant);
    }
    tos = atos;
  } else {
    if (tag.is_float()) {
        // ftos
        __ movptr(r0, (intptr_t)method->constants()->float_at_addr(idx));
        __ ldrs(v0, Address(r0, 0));
        tos = ftos;
    } else {
        // itos JVM_CONSTANT_Integer only
        __ mov(r0, method->constants()->int_at(idx));
        tos = itos;
    }
  }
}

void NormalCompileTask::ldc2_w() {
    transition(vtos, vtos);
    if (tos != vtos) {
        __ push(tos);
    }

    int idx = bs->get_index_u2();
    constantTag tag = method->constants()->tag_at(idx);
    if (tag.is_double()) {
        __ movptr(r3, (intptr_t)method->constants()->double_at_addr(idx));
        __ ldrd(v0, Address(r3, 0));
        tos = dtos;
    } else if (tag.is_long()) {
        __ movptr(r0, (intptr_t)method->constants()->long_at_addr(idx));
        __ ldr(r0, Address(r0, 0));
        tos = ltos;
    } else {
        ShouldNotReachHere();
    }

}

void NormalCompileTask::locals_index(Register reg, int offset)
{
    __ ldrb(reg, at_bcp(offset));
    __ neg(reg, reg);
}

void NormalCompileTask::iload() {
  transition(vtos, itos);
  __ ldr(r0, iaddress(bs->get_index_u1()));
}
void NormalCompileTask::iload(int n) {
  transition(vtos, itos);
  __ ldr(r0, iaddress(n));
}
void NormalCompileTask::lload() {
  transition(vtos, ltos);
  __ ldr(r0, laddress(bs->get_index_u1()));
}
void NormalCompileTask::lload(int n) {
  transition(vtos, ltos);
  __ ldr(r0, laddress(n));
}
void NormalCompileTask::fload() {
  transition(vtos, ftos);
  __ ldrd(v0, faddress(bs->get_index_u1()));
}
void NormalCompileTask::fload(int n) {
  transition(vtos, ftos);
  __ ldrd(v0, faddress(n));
}
void NormalCompileTask::dload() {
  __ ldrd(v0, daddress(bs->get_index_u1()));
}
void NormalCompileTask::dload(int n) {
  __ ldrd(v0, daddress(n));
}
void NormalCompileTask::aload() {
  __ ldr(r0, aaddress(bs->get_index_u1()));
}
void NormalCompileTask::aload(int n) {
  __ ldr(r0, aaddress(n));
}
void NormalCompileTask::aload_0() {
  aload(0);
}

void NormalCompileTask::iaload() {
  __ pop_ptr(r2);
  // r0: index
  // r1: array
  index_check(r2, r0); // kills rbx
  __ lea(r2, Address(r2, r0, Address::uxtw(2)));
  __ ldrw(r0, Address(r2, arrayOopDesc::base_offset_in_bytes(T_INT)));
}
void NormalCompileTask::laload() {
  __ pop_ptr(r2);
  // r0: index
  // r1: array
  index_check(r2, r0); // kills rbx
  __ lea(r2, Address(r2, r0, Address::uxtw(3)));
  __ ldr(r0, Address(r2,  arrayOopDesc::base_offset_in_bytes(T_LONG)));
}
void NormalCompileTask::faload() {
  __ pop_ptr(r2);
  // r0: index
  // r1: array
  index_check(r2, r0); // kills rbx
  __ lea(r2,  Address(r2, r0, Address::uxtw(2)));
  __ ldrs(v0, Address(r2,  arrayOopDesc::base_offset_in_bytes(T_FLOAT)));
}
void NormalCompileTask::daload() {
  __ pop_ptr(r2);
  // r0: index
  // r1: array
  index_check(r2, r0); // kills rbx
  __ lea(r2,  Address(r2, r0, Address::uxtw(3)));
  __ ldrd(v0, Address(r2,  arrayOopDesc::base_offset_in_bytes(T_DOUBLE)));
}
void NormalCompileTask::aaload() {
  __ pop_ptr(r2);
  // r0: index
  // r1: array
  index_check(r2, r0); // kills rbx
  int s = (UseCompressedOops ? 2 : 3);
  __ lea(r2, Address(r2, r0, Address::uxtw(s)));
  __ load_heap_oop(r0, Address(r2, arrayOopDesc::base_offset_in_bytes(T_OBJECT)));
}
void NormalCompileTask::baload() {
  __ pop_ptr(r2);
  // eax: index
  // rdx: array
  index_check(r2, r0); // kills rbx
  __ lea(r2,  Address(r2, r0, Address::uxtw(0)));
  __ load_signed_byte(r0, Address(r2,  arrayOopDesc::base_offset_in_bytes(T_BYTE)));
}
void NormalCompileTask::caload() {
  __ pop_ptr(r2);
  // eax: index
  // rdx: array
  index_check(r2, r0); // kills rbx
  __ lea(r2,  Address(r2, r0, Address::uxtw(1)));
  __ load_unsigned_short(r0, Address(r2,  arrayOopDesc::base_offset_in_bytes(T_CHAR)));

}
void NormalCompileTask::saload() {
  __ pop_ptr(r2);
  // eax: index
  // rdx: array
  index_check(r2, r0); // kills rbx
  __ lea(r2,  Address(r1, r0, Address::uxtw(1)));
  __ load_signed_short(r0, Address(r2,  arrayOopDesc::base_offset_in_bytes(T_SHORT)));
}
void NormalCompileTask::istore(){
  __ strw(r0, iaddress(bs->get_index_u1()));
}
void NormalCompileTask::istore(int n){
  __ strw(r0, iaddress(n));
}
void NormalCompileTask::lstore(){
  __ str(r0, laddress(bs->get_index_u1()));
}
void NormalCompileTask::lstore(int n){
  __ str(r0, laddress(n));
}
void NormalCompileTask::fstore(){
  __ strs(v0, faddress(bs->get_index_u1()));
}
void NormalCompileTask::fstore(int n){
  __ strs(v0, faddress(n));
}
void NormalCompileTask::dstore(){
  __ strd(v0, daddress(bs->get_index_u1()));
}
void NormalCompileTask::dstore(int n){
  __ strd(v0, daddress(n));
}
void NormalCompileTask::astore(){
    __ current_entry->set_bit(bs->get_index_u1(), C0MapOopEntry::oop_bit_number);
    __ str(r0, aaddress(bs->get_index_u1()));
}
void NormalCompileTask::astore(int n){
    __ current_entry->set_bit(n, C0MapOopEntry::oop_bit_number);
    __ str(r0, aaddress(n));
}
void NormalCompileTask::iastore() {
  __ pop_i(r1);
  __ pop_ptr(r3);
  // r0: value
  // r1: index
  // r3: array
  index_check(r3, r1); // prefer index in r1
  //oopDesc::bs()->interpreter_write_barrier(_masm, r3);
  __ lea(rscratch1, Address(r3, r1, Address::uxtw(2)));
  __ strw(r0, Address(rscratch1,
		      arrayOopDesc::base_offset_in_bytes(T_INT)));
}
void NormalCompileTask::lastore() {
  transition(ltos, vtos);
  __ pop_i(r1);
  __ pop_ptr(r3);
  // r0: value
  // r1: index
  // r3: array
  index_check(r3, r1); // prefer index in r1
  //oopDesc::bs()->interpreter_write_barrier(_masm, r3);
  __ lea(rscratch1, Address(r3, r1, Address::uxtw(3)));
  __ str(r0, Address(rscratch1,
		      arrayOopDesc::base_offset_in_bytes(T_LONG)));
}
void NormalCompileTask::fastore() {
  transition(ftos, vtos);
  __ pop_i(r1);
  __ pop_ptr(r3);
  // v0: value
  // r1:  index
  // r3:  array
  index_check(r3, r1); // prefer index in r1
  //oopDesc::bs()->interpreter_write_barrier(_masm, r3);
  __ lea(rscratch1, Address(r3, r1, Address::uxtw(2)));
  __ strs(v0, Address(rscratch1,
		      arrayOopDesc::base_offset_in_bytes(T_FLOAT)));
}
void NormalCompileTask::dastore() {
  transition(dtos, vtos);
  __ pop_i(r1);
  __ pop_ptr(r3);
  // v0: value
  // r1:  index
  // r3:  array
  index_check(r3, r1); // prefer index in r1
  //oopDesc::bs()->interpreter_write_barrier(_masm, r3);
  __ lea(rscratch1, Address(r3, r1, Address::uxtw(3)));
  __ strd(v0, Address(rscratch1,
		      arrayOopDesc::base_offset_in_bytes(T_DOUBLE)));
}
void NormalCompileTask::aastore() {
    if (tos != vtos) {
      __ push(tos);
      tos = vtos;
    }
    Label is_null, ok_is_subtype, done;
    transition(vtos, vtos);
    // stack: ..., array, index, value
    __ ldr(r0, at_tos());    // value
    __ ldr(r2, at_tos_p1()); // index
    __ ldr(r3, at_tos_p2()); // array

    Address element_address(r4, arrayOopDesc::base_offset_in_bytes(T_OBJECT));

    index_check(r3, r2);     // kills r1
    // oopDesc::bs()->interpreter_write_barrier(_masm, r3);
    __ lea(r4, Address(r3, r2, Address::uxtw(UseCompressedOops? 2 : 3)));

    // do array store check - check for NULL value first
    __ cbz(r0, is_null);

    // Move subklass into r1
    __ load_klass(r1, r0);
    // Move superklass into r0
    __ load_klass(r0, r3);
    __ ldr(r0, Address(r0,
                       ObjArrayKlass::element_klass_offset()));
    // Compress array + index*oopSize + 12 into a single register.  Frees r2.

    // Generate subtype check.  Blows r2, r5
    // Superklass in r0.  Subklass in r1.
    __ gen_subtype_check(r1, ok_is_subtype);

    // Come here on failure
    // object is at TOS
    __ b(Interpreter::_throw_ArrayStoreException_entry);

    // Come here on success
    __ bind(ok_is_subtype);

    // Get the value we will store
    __ ldr(r0, at_tos());
    // Now store using the appropriate barrier
    do_oop_store(_masm, element_address, r0, BarrierSet::Other, true);
    __ b(done);

    // Have a NULL in r0, r3=array, r2=index.  Store NULL at ary[idx]
    __ bind(is_null);
    __ profile_null_seen(r2);

    // Store a NULL
    do_oop_store(_masm, element_address, noreg, BarrierSet::Other, true);

    // Pop stack arguments
    __ bind(done);
    __ add(esp, esp, 3 * Interpreter::stackElementSize);
}
void NormalCompileTask::bastore() {
  transition(itos, vtos);
  __ pop_i(r1);
  __ pop_ptr(r3);
  // r0: value
  // r1: index
  // r3: array
  //oopDesc::bs()->interpreter_write_barrier(_masm, r3);
  index_check(r3, r1); // prefer index in r1

  // Need to check whether array is boolean or byte
  // since both types share the bastore bytecode.
  __ load_klass(r2, r3);
  __ ldrw(r2, Address(r2, Klass::layout_helper_offset()));
  int diffbit = Klass::layout_helper_boolean_diffbit();
  __ andw(rscratch1, r2, diffbit);
  Label L_skip;
  __ cbzw(rscratch1, L_skip);
  __ andw(r0, r0, 1);  // if it is a T_BOOLEAN array, mask the stored value to 0/1
  __ bind(L_skip);

  __ lea(rscratch1, Address(r3, r1, Address::uxtw(0)));
  __ strb(r0, Address(rscratch1,
		      arrayOopDesc::base_offset_in_bytes(T_BYTE)));
}
void NormalCompileTask::castore() {
  transition(itos, vtos);
  __ pop_i(r1);
  __ pop_ptr(r3);
  // r0: value
  // r1: index
  // r3: array
  index_check(r3, r1); // prefer index in r1
  //oopDesc::bs()->interpreter_write_barrier(_masm, r3);
  __ lea(rscratch1, Address(r3, r1, Address::uxtw(1)));
  __ strh(r0, Address(rscratch1,
		      arrayOopDesc::base_offset_in_bytes(T_CHAR)));
}
void NormalCompileTask::sastore() {
    castore();
}

void NormalCompileTask::pop() {
  transition(vtos, vtos);
  if (tos == vtos)
    __ add(esp, esp, Interpreter::stackElementSize);
}
void NormalCompileTask::pop2() {
  transition(vtos, vtos);
  if (tos == vtos)
    __ add(esp, esp, 2 * Interpreter::stackElementSize);
  else
  __ add(esp, esp, Interpreter::stackElementSize);
}
void NormalCompileTask::dup() {
  transition(vtos, vtos);
  if (tos == vtos) {
    __ ldr(r0, Address(esp, 0));
    tos = atos;
  } else {
    __ push(r0);
  }
  // stack: ..., a, a
}
void NormalCompileTask::dup_x1() {
  transition(vtos, vtos);
  // stack: ..., a, b
  if (tos == vtos) {
    __ ldr(r0, at_tos());  // load b
    __ ldr(r2, at_tos_p1());  // load a
    __ str(r0, at_tos_p1());  // store b
    __ str(r2, at_tos());  // store a
    __ push(r0);      // push b
    // stack: ..., b, a, b
  } else {
    __ ldr(r2, at_tos());
    __ str(r0, at_tos());
    __ push(r2);
  }
}
void NormalCompileTask::dup_x2() {
  transition(vtos, vtos);
  // stack: ..., a, b, c
  if (tos != vtos) {
    __ push(tos);
    tos = vtos;
  }
  __ ldr(r0, at_tos());  // load c
  __ ldr(r2, at_tos_p2());  // load a
  __ str(r0, at_tos_p2());  // store c in a
  __ push(r0);      // push c
  // stack: ..., c, b, c, c
  __ ldr(r0, at_tos_p2());  // load b
  __ str(r2, at_tos_p2());  // store a in b
  // stack: ..., c, a, c, c
  __ str(r0, at_tos_p1());  // store b in c
  // stack: ..., c, a, b, c
}
void NormalCompileTask::dup2() {
  transition(vtos, vtos);
  // stack: ..., a, b
  if (tos != vtos) {
    __ push(tos);
    tos = vtos;
  }
  __ ldr(r0, at_tos_p1());  // load a
  __ push(r0);                  // push a
  __ ldr(r0, at_tos_p1());  // load b
  __ push(r0);                  // push b
  // stack: ..., a, b, a, b
}
void NormalCompileTask::dup2_x1(){
  transition(vtos, vtos);
  // stack: ..., a, b, c
  if (tos != vtos) {
    __ push(tos);
    tos = vtos;
  }
  __ ldr(r2, at_tos());  // load c
  __ ldr(r0, at_tos_p1());  // load b
  __ push(r0);                  // push b
  __ push(r2);                  // push c
  // stack: ..., a, b, c, b, c
  __ str(r2, at_tos_p3());  // store c in b
  // stack: ..., a, c, c, b, c
  __ ldr(r2, at_tos_p4());  // load a
  __ str(r2, at_tos_p2());  // store a in 2nd c
  // stack: ..., a, c, a, b, c
  __ str(r0, at_tos_p4());  // store b in a
  // stack: ..., b, c, a, b, c
}
void NormalCompileTask::dup2_x2(){
  transition(vtos, vtos);
  // stack: ..., a, b, c, d
  if (tos != vtos) {
    __ push(tos);
    tos = vtos;
  }
  __ ldr(r2, at_tos());  // load d
  __ ldr(r0, at_tos_p1());  // load c
  __ push(r0)            ;      // push c
  __ push(r2);                  // push d
  // stack: ..., a, b, c, d, c, d
  __ ldr(r0, at_tos_p4());  // load b
  __ str(r0, at_tos_p2());  // store b in d
  __ str(r2, at_tos_p4());  // store d in b
  // stack: ..., a, d, c, b, c, d
  __ ldr(r2, at_tos_p5());  // load a
  __ ldr(r0, at_tos_p3());  // load c
  __ str(r2, at_tos_p3());  // store a in c
  __ str(r0, at_tos_p5());  // store c in a
  // stack: ..., c, d, a, b, c, d
}
void NormalCompileTask::swap(){
  transition(vtos, vtos);
  // stack: ..., a, b
  if (tos == vtos) {
    __ ldr(r2, at_tos_p1());  // load a
    __ ldr(r0, at_tos());  // load b
    __ str(r2, at_tos());  // store a in b
    __ str(r0, at_tos_p1());  // store b in a
    // stack: ..., b, a
  } else {
    __ ldr(r2, at_tos());
    __ str(r0, at_tos());
    __ mov(r0, r2);
  }
}
void NormalCompileTask::iop2(Operation op){
  transition(itos, itos);
  // r0 <== r1 op r0
  __ pop_i(r1);
  switch (op) {
  case add  : __ addw(r0, r1, r0); break;
  case sub  : __ subw(r0, r1, r0); break;
  case mul  : __ mulw(r0, r1, r0); break;
  case _and : __ andw(r0, r1, r0); break;
  case _or  : __ orrw(r0, r1, r0); break;
  case _xor : __ eorw(r0, r1, r0); break;
  case shl  : __ lslvw(r0, r1, r0); break;
  case shr  : __ asrvw(r0, r1, r0); break;
  case ushr : __ lsrvw(r0, r1, r0);break;
  default   : ShouldNotReachHere();
  }
}
void NormalCompileTask::lop2(Operation op){
  transition(ltos, ltos);
  // r0 <== r1 op r0
  __ pop_l(r1);
  switch (op) {
  case add  : __ add(r0, r1, r0); break;
  case sub  : __ sub(r0, r1, r0); break;
  case mul  : __ mul(r0, r1, r0); break;
  case _and : __ andr(r0, r1, r0); break;
  case _or  : __ orr(r0, r1, r0); break;
  case _xor : __ eor(r0, r1, r0); break;
  default   : ShouldNotReachHere();
  }
}
void NormalCompileTask::fop2(Operation op){
  transition(ftos, ftos);
  switch (op) {
  case add:
    // n.b. use ldrd because this is a 64 bit slot
    __ pop_f(v1);
    __ fadds(v0, v1, v0);
    break;
  case sub:
    __ pop_f(v1);
    __ fsubs(v0, v1, v0);
    break;
  case mul:
    __ pop_f(v1);
    __ fmuls(v0, v1, v0);
    break;
  case div:
    __ pop_f(v1);
    __ fdivs(v0, v1, v0);
    break;
  case rem:
    __ fmovs(v1, v0);
    __ pop_f(v0);
    __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::frem));
    break;
  default:
    ShouldNotReachHere();
    break;
  }
}
void NormalCompileTask::dop2(Operation op){
  transition(dtos, dtos);
  switch (op) {
  case add:
    // n.b. use ldrd because this is a 64 bit slot
    __ pop_d(v1);
    __ faddd(v0, v1, v0);
    break;
  case sub:
    __ pop_d(v1);
    __ fsubd(v0, v1, v0);
    break;
  case mul:
    __ pop_d(v1);
    __ fmuld(v0, v1, v0);
    break;
  case div:
    __ pop_d(v1);
    __ fdivd(v0, v1, v0);
    break;
  case rem:
    __ fmovd(v1, v0);
    __ pop_d(v0);
    __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::drem));
    break;
  default:
    ShouldNotReachHere();
    break;
  }
}

void NormalCompileTask::irem(){
  transition(itos, itos);
  // explicitly check for div0
  Label no_div0;
  __ cbnzw(r0, no_div0);
  __ mov(rscratch1, Interpreter::_throw_ArithmeticException_entry);
  __ br(rscratch1);
  __ bind(no_div0);
  __ pop_i(r1);
  // r0 <== r1 irem r0
  __ corrected_idivl(r0, r1, r0, true);
}
void NormalCompileTask::lrem(){
  transition(ltos, ltos);
  // explicitly check for div0
  Label no_div0;
  __ cbnz(r0, no_div0);
  __ mov(rscratch1, Interpreter::_throw_ArithmeticException_entry);
  __ br(rscratch1);
  __ bind(no_div0);
  __ pop_l(r1);
  // r0 <== r1 lrem r0
  __ corrected_idivq(r0, r1, r0,  true);
}
void NormalCompileTask::ineg(){
  transition(itos, itos);
  __ negw(r0, r0);
}
void NormalCompileTask::lneg(){
  transition(ltos, ltos);
  __ neg(r0, r0);
}
void NormalCompileTask::fneg(){
  transition(ftos, ftos);
  __ fnegs(v0, v0);
}
void NormalCompileTask::dneg(){
  transition(dtos, dtos);
  __ fnegd(v0, v0);
}
void NormalCompileTask::lshl(){
  transition(itos, ltos);
  // shift count is in r0
  __ pop_l(r1);
  __ lslv(r0, r1, r0);
}
void NormalCompileTask::lshr(){
  transition(itos, ltos);
  // shift count is in r0
  __ pop_l(r1);
  __ asrv(r0, r1, r0);
}
void NormalCompileTask::lushr(){
  transition(itos, ltos);
  // shift count is in r0
  __ pop_l(r1);
  __ lsrv(r0, r1, r0);
}
void NormalCompileTask::convert(){
  // Checking
#ifdef ASSERT
  {
    TosState tos_in  = ilgl;
    TosState tos_out = ilgl;
    switch (bs->code()) {
    case Bytecodes::_i2l: // fall through
    case Bytecodes::_i2f: // fall through
    case Bytecodes::_i2d: // fall through
    case Bytecodes::_i2b: // fall through
    case Bytecodes::_i2c: // fall through
    case Bytecodes::_i2s: tos_in = itos; break;
    case Bytecodes::_l2i: // fall through
    case Bytecodes::_l2f: // fall through
    case Bytecodes::_l2d: tos_in = ltos; break;
    case Bytecodes::_f2i: // fall through
    case Bytecodes::_f2l: // fall through
    case Bytecodes::_f2d: tos_in = ftos; break;
    case Bytecodes::_d2i: // fall through
    case Bytecodes::_d2l: // fall through
    case Bytecodes::_d2f: tos_in = dtos; break;
    default             : ShouldNotReachHere();
    }
    switch (bs->code()) {
    case Bytecodes::_l2i: // fall through
    case Bytecodes::_f2i: // fall through
    case Bytecodes::_d2i: // fall through
    case Bytecodes::_i2b: // fall through
    case Bytecodes::_i2c: // fall through
    case Bytecodes::_i2s: tos_out = itos; break;
    case Bytecodes::_i2l: // fall through
    case Bytecodes::_f2l: // fall through
    case Bytecodes::_d2l: tos_out = ltos; break;
    case Bytecodes::_i2f: // fall through
    case Bytecodes::_l2f: // fall through
    case Bytecodes::_d2f: tos_out = ftos; break;
    case Bytecodes::_i2d: // fall through
    case Bytecodes::_l2d: // fall through
    case Bytecodes::_f2d: tos_out = dtos; break;
    default             : ShouldNotReachHere();
    }
    transition(tos_in, tos_out);
  }
#endif // ASSERT
  // static const int64_t is_nan = 0x8000000000000000L;

  // Conversion
  switch (bs->code()) {
  case Bytecodes::_i2l:
    __ sxtw(r0, r0);
    break;
  case Bytecodes::_i2f:
    __ scvtfws(v0, r0);
    break;
  case Bytecodes::_i2d:
    __ scvtfwd(v0, r0);
    break;
  case Bytecodes::_i2b:
    __ sxtbw(r0, r0);
    break;
  case Bytecodes::_i2c:
    __ uxthw(r0, r0);
    break;
  case Bytecodes::_i2s:
    __ sxthw(r0, r0);
    break;
  case Bytecodes::_l2i:
    __ uxtw(r0, r0);
    break;
  case Bytecodes::_l2f:
    __ scvtfs(v0, r0);
    break;
  case Bytecodes::_l2d:
    __ scvtfd(v0, r0);
    break;
  case Bytecodes::_f2i:
  {
    Label L_Okay;
    __ clear_fpsr();
    __ fcvtzsw(r0, v0);
    __ get_fpsr(r1);
    __ cbzw(r1, L_Okay);
    __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::f2i));
    __ bind(L_Okay);
  }
    break;
  case Bytecodes::_f2l:
  {
    Label L_Okay;
    __ clear_fpsr();
    __ fcvtzs(r0, v0);
    __ get_fpsr(r1);
    __ cbzw(r1, L_Okay);
    __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::f2l));
    __ bind(L_Okay);
  }
    break;
  case Bytecodes::_f2d:
    __ fcvts(v0, v0);
    break;
  case Bytecodes::_d2i:
  {
    Label L_Okay;
    __ clear_fpsr();
    __ fcvtzdw(r0, v0);
    __ get_fpsr(r1);
    __ cbzw(r1, L_Okay);
    __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::d2i));
    __ bind(L_Okay);
  }
    break;
  case Bytecodes::_d2l:
  {
    Label L_Okay;
    __ clear_fpsr();
    __ fcvtzd(r0, v0);
    __ get_fpsr(r1);
    __ cbzw(r1, L_Okay);
    __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::d2l));
    __ bind(L_Okay);
  }
    break;
  case Bytecodes::_d2f:
    __ fcvtd(v0, v0);
    break;
  default:
    ShouldNotReachHere();
  }
}

void NormalCompileTask::lcmp(){
  transition(ltos, itos);
  Label done;
  __ pop_l(r1);
  __ cmp(r1, r0);
  __ mov(r0, (u_int64_t)-1L);
  __ br(Assembler::LT, done);
  // __ mov(r0, 1UL);
  // __ csel(r0, r0, zr, Assembler::NE);
  // and here is a faster way
  __ csinc(r0, zr, zr, Assembler::EQ);
  __ bind(done);
}
void NormalCompileTask::float_cmp(int unordered_result) {
    float_cmp(true, unordered_result);
}
void NormalCompileTask::float_cmp(bool is_float, int unordered_result) {
  Label done;
  if (is_float) {
    // XXX get rid of pop here, use ... reg, mem32
    __ pop_f(v1);
    __ fcmps(v1, v0);
  } else {
    // XXX get rid of pop here, use ... reg, mem64
    __ pop_d(v1);
    __ fcmpd(v1, v0);
  }
  if (unordered_result < 0) {
    // we want -1 for unordered or less than, 0 for equal and 1 for
    // greater than.
    __ mov(r0, (u_int64_t)-1L);
    // for FP LT tests less than or unordered
    __ br(Assembler::LT, done);
    // install 0 for EQ otherwise 1
    __ csinc(r0, zr, zr, Assembler::EQ);
  } else {
    // we want -1 for less than, 0 for equal and 1 for unordered or
    // greater than.
    __ mov(r0, 1L);
    // for FP HI tests greater than or unordered
    __ br(Assembler::HI, done);
    // install 0 for EQ otherwise ~0
    __ csinv(r0, zr, zr, Assembler::EQ);

  }
  __ bind(done);
}
void NormalCompileTask::double_cmp(int unordered_result) {
    float_cmp(false, unordered_result);
}
void NormalCompileTask::if_0cmp(NormalCompileTask::Condition cc) {
  transition(itos, vtos);
  // assume branch is more often taken than not (loops use backward branches)
  Label not_taken;
  if (cc == equal)
    __ cbnzw(r0, not_taken);
  else if (cc == notEqual)
    __ cbzw(r0, not_taken);
  else {
    __ andsw(zr, r0, r0);
    __ br(j_not(cc), not_taken);
  }

  branch(false, false, none);
  __ bind(not_taken);
  __ profile_not_taken_branch(r0);
}

// TODO: if_icmp and if_acmp is not efficient, as they need to jump twice
void NormalCompileTask::if_icmp(NormalCompileTask::Condition cc) {
  transition(itos, vtos);
  // assume branch is more often taken than not (loops use backward branches)
  Label not_taken;
  __ pop_i(r1);
  __ cmpw(r1, r0, Assembler::LSL);
  __ br(j_not(cc), not_taken);
  branch(false, false, none);
  __ bind(not_taken);
  __ profile_not_taken_branch(r0);
}

void NormalCompileTask::if_acmp(NormalCompileTask::Condition cc) {
  transition(atos, vtos);
  // assume branch is more often taken than not (loops use backward branches)
  Label not_taken;
  __ pop_ptr(r1);
  __ cmpoops(r1, r0);
  __ br(j_not(cc), not_taken);
  branch(false, false, none);
  __ bind(not_taken);
  __ profile_not_taken_branch(r0);
}

void NormalCompileTask::if_nullcmp(NormalCompileTask::Condition cc) {
  transition(atos, vtos);
  // assume branch is more often taken than not (loops use backward branches)
  Label not_taken;
  if (cc == equal)
    __ cbnz(r0, not_taken);
  else
    __ cbz(r0, not_taken);
  branch(false, false, none);
  __ bind(not_taken);
  __ profile_not_taken_branch(r0);
}
//not found in aarch64 template table
void NormalCompileTask::_goto() {
    branch(false, false, none);
    tos = udtos;
}
void NormalCompileTask::jsr(){
    Unimplemented();
}
void NormalCompileTask::ret(){
  transition(vtos, vtos);
  Unimplemented();
}
void NormalCompileTask::iinc(){
  transition(vtos, vtos);
  signed char* value_pointer = (signed char*)bs->bcp() + 2;
  __ ldr(r0, iaddress(bs->get_index_u1()));
  __ addw(r0, r0, *value_pointer);
  __ str(r0, iaddress(bs->get_index_u1()));
}
void NormalCompileTask::wide_iinc() {
  D_WARN_Unimplement;
}
void NormalCompileTask::branch(bool is_jsr, bool is_wide, Condition c){
  // We might be moving to a safepoint.  The thread which calls
  // Interpreter::notice_safepoints() will effectively flush its cache
  // when it makes a system call, but we need to do something to
  // ensure that we see the changed dispatch table.
  __ membar(MacroAssembler::LoadLoad);

  int target = 0;

  // load branch displacement
  if (is_wide)
    target = bs->get_far_dest();
  else
    target = bs->get_dest();


  // Handle all the JSR stuff here, then exit.
  // It's much shorter and cleaner than intermingling with the non-JSR
  // normal-branch stuff occurring below.

  if (is_jsr)
    Unimplemented();

  jump_target(target, c);
  will_run = false;
}
void NormalCompileTask::lmul() {
  transition(ltos, ltos);
  __ pop_l(r1);
  __ mul(r0, r0, r1);
}
void NormalCompileTask::ldiv(){
  transition(ltos, ltos);
  // explicitly check for div0
  Label no_div0;
  __ cbnz(r0, no_div0);
  __ mov(rscratch1, Interpreter::_throw_ArithmeticException_entry);
  __ br(rscratch1);
  __ bind(no_div0);
  __ pop_l(r1);
  // r0 <== r1 ldiv r0
  __ corrected_idivq(r0, r1, r0, false);
}
void NormalCompileTask::idiv(){
  transition(itos, itos);
  // explicitly check for div0
  Label no_div0;
  __ cbnzw(r0, no_div0);
  __ mov(rscratch1, Interpreter::_throw_ArithmeticException_entry);
  __ br(rscratch1);
  __ bind(no_div0);
  __ pop_i(r1);
  // r0 <== r1 idiv r0
  __ corrected_idivl(r0, r1, r0, false);
}

void NormalCompileTask::entry() {
    // determine code generation flags

    bool inc_counter = false;

    // ebx: Method*
    // r13: sender sp
    this->method_entry = __ pc();

    int size_locals = method->max_locals();
    int size_parameters = method->size_of_parameters();
    int addtional_locals = size_locals - size_parameters;

    if (break_method != NULL &&
        strcmp(method->name()->as_C_string(), break_method) == 0 &&
        strcmp(method->klass_name()->as_C_string(), break_klass) == 0) {
        // __ os_breakpoint();
    }


    // see if we've got enough room on the stack for locals plus overhead.
//    generate_stack_overflow_check();


    // r11 changed to r15 because all are caller saved last register
    // is ecall
    if (is_sgx_interface(method) || strncmp(method->name()->as_C_string(), "sgx_hook", 8) == 0) {
        for (int i = 0;i < size_parameters;i++)
        {
            // movptr change to str, times8 change to lsl(3)
            __ ldr(r15, Address(rlocals, - i * wordSize));
            __ push(r15);
        }
    }

    // compute beginning of parameters (rlocals)
    __ add(rlocals, esp, size_parameters * wordSize);
    __ sub(rlocals, rlocals, wordSize);

    //Add because it is in hotspot/enclave/cpu/aarch64/vm/templateInterpreter_aarch64.cpp
    // Make room for locals
    __ sub(rscratch1, esp, addtional_locals * wordSize);
    __ andr(sp, rscratch1, -16);

    // r3 - # of additional locals
    // allocate space for locals
    // explicitly initialize locals
    __ ands(zr, r3, r3);
    for (int i = 0;i < addtional_locals;i++)
    {
        __ str(zr, Address(__ post(rscratch1, wordSize)));// initialize local variables
    }

    // initialize fixed part of activation frame
    generate_fixed_frame(false);

    // check for synchronized methods
    // Must happen AFTER invocation_counter check and stack overflow check,
    // so method is not locked if overflows.
    // if (synchronized) {
    //   // Allocate monitor and lock method
    //   __ lock_method();
    // }
    BREAK_IF(method) {
      __ lea(rscratch1, RuntimeAddress(Runtime0::entry_for(Runtime0::debug_method_entry_id)));
      __ blr(rscratch1);
    }
}

// address TemplateInterpreterGenerator::generate_return_entry_for(TosState state, int step, size_t index_size)
void NormalCompileTask::return_entry(TosState state, int parameter_size) {

    // Restore stack bottom in case i2c adjusted stack
    __ ldr(esp, Address(rfp, frame::interpreter_frame_last_sp_offset * wordSize));
    // and NULL it as marker that esp is now tos until next java call
    __ str(zr, Address(rfp, frame::interpreter_frame_last_sp_offset * wordSize));

    __ restore_bcp();
    __ restore_locals();

    __ add(esp, esp, parameter_size * wordSize);

    // the parameters plus the ret address
    __ current_entry->clear_bit_prev(parameter_size + 1);
}
//TODO: clean the code
void NormalCompileTask::_return(TosState state) {

    BREAK_IF(method) {
      __ movptr(rmethod, (intptr_t) method);
      __ lea(rscratch1, RuntimeAddress(Runtime0::entry_for(Runtime0::debug_method_exit_id)));
      __ blr(rscratch1);
    }

    if (ret_tos == vtos)
      __ pop(state);

    // Narrow result if state is itos but result type is smaller.
    // Need to narrow in the return bytecode rather than in generate_return_entry
    // since compiled code callers expect the result to already be narrowed.
    narrow(r0, state);
    remove_activation(state, r13, true, true, true);

    __ ret(lr);
    tos = state;
}

static inline int32_t extend(unsigned val, int hi = 31, int lo = 0) {
  union {
      unsigned u;
      int n;
  };

  u = val << (31 - hi);
  n = n >> (31 - hi + lo);
  return n;
}

void NormalCompileTask::patch_jmp(address inst_addr, address jmp_addr) {
  // support only unconditional jump now
  uint32_t inst = *(uint32_t*)inst_addr;
  if (Instruction_aarch64::extract(inst, 31, 26) == 0b000101) {
    long offset = (long) (jmp_addr - inst_addr) >> 2;
    Instruction_aarch64::patch(inst_addr, 25, 0, extend(offset, 25, 0));
  } else {
    // this is not unconditional jump
    D_WARN_Unimplement;
  }
}

void NormalCompileTask::index_check(Register array, Register index) {
    // destroys r1, rscratch1
    // check array
    __ null_check(array, arrayOopDesc::length_offset_in_bytes());
    // sign extend index for use by indexed load
    // __ movl2ptr(index, index);
    // check index
    Register length = rscratch1;
    __ ldrw(length, Address(array, arrayOopDesc::length_offset_in_bytes()));
    __ cmpw(index, length);
    if (index != r1) {
        // ??? convention: move aberrant index into r1 for exception message
        assert(r1 != array, "different registers");
        __ mov(r1, index);
    }
    Label ok;
    __ br(Assembler::LO, ok);
    __ mov(rscratch1, Interpreter::_throw_ArrayIndexOutOfBoundsException_entry);
    __ br(rscratch1);
    __ bind(ok);
}

//-----------------------------------------------------------------------------
// Allocation

void NormalCompileTask::_new() {
    transition(vtos, atos);
    int idx = bs->get_index_u2();
    constantTag tag =  method->constants()->tag_at(idx);
    if (!tag.is_klass()) {
        if (will_run) {
            Bytecode_new bytecode_new(method, method->bcp_from(bs->bci()));
            Klass* klass = method->constants()->klass_at(bytecode_new.index(), JavaThread::current());
            __ movptr(r3, (intptr_t) klass);
        } else {
            // do the patch
            PatchingStub *patchingStub = new PatchingStub(_masm, PatchingStub::load_klass_id, bs->bci());
            __ movptr(r3, NULL_WORD);
            patchingStub->install();
            append_stub(patchingStub);
        }
    } else {
        Klass* klass = method->constants()->klass_at_if_loaded(constantPoolHandle(method->constants()), idx);
        __ movptr(r3, (intptr_t) klass);
    }
    // check gc_point() implementation
    gc_point();

    //__ call(RuntimeAddress(Runtime0::entry_for(Runtime0::new_instance_id)));
    //call to below
    __ lea(r0, RuntimeAddress(Runtime0::entry_for(Runtime0::new_instance_id)));
    __ blr(r0);
}

void NormalCompileTask::newarray() {
    Register length = r19;
    Register klass  = r3;

    gc_point();
    __ mov(length, r0);
    __ movptr(klass, (intptr_t)Universe::typeArrayKlassObj()[bs->get_index_u1()]);

    gc_point();

    //__ call(RuntimeAddress(Runtime0::entry_for(Runtime0::new_type_array_id)));
    //call to below
    __ lea(r0, RuntimeAddress(Runtime0::entry_for(Runtime0::new_type_array_id)));
    __ blr(r0);
}

void NormalCompileTask::anewarray() {
    transition(itos, atos);
    gc_point();
    int idx = bs->get_index_u2();
    __ mov(r19, r0);
    constantTag tag =  method->constants()->tag_at(idx);
    if (!tag.is_klass()) {
        if (will_run) {
            Bytecode_anewarray anew(method, method->bcp_from(bs->bci()));
            Klass* ek = method->constants()->klass_at(anew.index(), JavaThread::current());
            Klass* klass = ek->array_klass(JavaThread::current());
            __ movptr(r3, (intptr_t)klass);
        } else {
            // do the patch
            PatchingStub *patchingStub = new PatchingStub(_masm, PatchingStub::load_klass_id, bs->bci());
            __ movptr(r3, NULL_WORD);
            patchingStub->install();
            append_stub(patchingStub);
        }
    } else {
        Klass* klass = method->constants()->klass_at_if_loaded(constantPoolHandle(method->constants()), idx);
        klass = (Klass*)klass->array_klass(JavaThread::current());
        __ movptr(r3, (intptr_t) klass);
    }

    gc_point();
    //__ call(RuntimeAddress(Runtime0::entry_for(Runtime0::new_object_array_id)));
    //call to below
    __ lea(r0, RuntimeAddress(Runtime0::entry_for(Runtime0::new_object_array_id)));
    __ blr(r0);
}

void NormalCompileTask::arraylength() {
    transition(atos, itos);
    __ null_check(r0, arrayOopDesc::length_offset_in_bytes());
    __ ldrw(r0, Address(r0, arrayOopDesc::length_offset_in_bytes()));
}

void NormalCompileTask::multianewarray() {
    //add gc point
    gc_point();
    transition(vtos, atos);
    int nfold = bs->bcp()[3]; // get number of dimensions
    // last dim is on top of stack; we want address of first one:
    // first_addr = last_addr + (ndims - 1) * wordSize
    __ lea(c_rarg3, Address(esp, (nfold - 1) * wordSize));

    // __ call_VME(CAST_FROM_FN_PTR(address, EnclaveMemory::static_cpoll_multi_array));
    //change below code to above

    __ mov(c_rarg0, rthread);
    __ movptr(c_rarg1, (intptr_t)method->constants());
    __ mov(c_rarg2, bs->get_index_u2());
    __ mov(c_rarg4, nfold);
    __ lea(rscratch1, RuntimeAddress(CAST_FROM_FN_PTR(address, EnclaveMemory::static_cpoll_multi_array)));
    __ blr(rscratch1);

    __ lea(esp, Address(esp, nfold * wordSize));
  __ current_entry->clear_bit_prev(nfold);
}


void NormalCompileTask::putfield(int byte_no) {
    putfield_or_static(byte_no, false);
}

void NormalCompileTask::putstatic(int byte_no) {
    putfield_or_static(byte_no, true);
}

void NormalCompileTask::getfield(int byte_no) {
    getfield_or_static(byte_no, false);
}

void NormalCompileTask::getstatic(int byte_no) {
    getfield_or_static(byte_no, true);
}

void NormalCompileTask::putfield_or_static(int byte_no, bool is_static) {
    //  1. resolved and put index into rdx, obj in rax, volatile??
    //  2. put the object
    // this expects tos is cached
    if (tos == vtos)
        __ pop(r0);

    const Register obj = r4;

    int off = 0xff;
    TosState tosState = ilgl;

    // we do not need to pop the field
    // __ pop(tos);

    // pop the object
    if (!is_static) {
        __ pop_ptr(obj);
        __ sgx_bound_check(obj, out_of_bound);
        has_bound_check = true;
    }

    PatchingStub *patching = resolve_cache_and_index(byte_no, obj, off, tosState, is_static);

    Address field(obj, off);
    switch(tosState) {
        case btos:  __ strb(r0, field); break;
        case ztos:  // __ andw(r0, r0, 0x1); this is comment out as patching is not handled, but we will need to handle this
                    __ strb(r0, field); break;
        case itos:  __ strw(r0, field); break;
        case ctos:  __ strh(r0, field); break;
        case stos:  __ strh(r0, field); break;
        case ltos:  __ str(r0, field); break;
        case ftos:  __ strs(v0, field); break;
        case dtos:  __ strd(v0, field); break;
        case atos:  __ store_heap_oop(field, r0); break;
        default:
            ShouldNotReachHere();
    }
    tos = vtos;
    if (patching) {
        patching->install();
        append_stub(patching);
    }
}

void NormalCompileTask::getfield_or_static(int byte_no, bool is_static) {
    // this expects tos is not cached
    // resolve the field and put the field id into cache
    // patch if not resolved
    // resolve_cache_and_index
    const Register obj = r4;
    int off = 0xff;
    TosState tosState = ilgl;
    if (!is_static) {
        assert_different_registers(obj, r0);
        if (tos == vtos) {
            __ pop(atos);
        }
        __ mov(obj, r0);
        __ sgx_bound_check(obj, out_of_bound);
        has_bound_check = true;
    } else {
        if (tos != vtos)
            __ push(tos);
    }
    PatchingStub* patching = resolve_cache_and_index(byte_no, obj, off, tosState, is_static);
    Address field(obj, off);
    // depends on the field type
    // should not be tos
    switch(tosState) {
        case btos: tos = btos; __ load_signed_byte(r0, field); break;
        case ztos: tos = ztos; __ ldrsb(r0, field); break;
        case itos: tos = itos; __ ldrw(r0, field); break;
        case ctos: tos = ctos; __ load_unsigned_short(r0, field); break;
        case stos: tos = stos; __ load_signed_short(r0, field); break;
        case ltos: tos = ltos; __ ldr(r0, field); break;
        case ftos: tos = ftos; __ ldrs(v0, field); break;
        case dtos: tos = dtos; __ ldrd(v0, field); break;
        case atos: tos = atos; __ load_heap_oop(r0, field); break;
        default:
            ShouldNotReachHere();
    }
    if (patching) {
        patching->install();
        append_stub(patching);
    }
}

// The registers cache and index expected to be set before call.
// Correct values of the cache and index registers are preserved.
void NormalCompileTask::jvmti_post_field_access(Register cache, Register index,
                                            bool is_static, bool has_tos) {
}

PatchingStub* NormalCompileTask::resolve_cache_and_index(int byte_no, Register c_obj, int &off, TosState &tosState, bool is_static) {
    // for loop of code
    Klass* klass_holder = method->method_holder();
    // load the address offset and the klass to be loaded,
    // it seems that putfield do not need to resolve it
    ConstantPoolCacheEntry *field_entry = method->constants()->cache()->entry_at(getfield_index());
    tosState = MetadataAccessor::basicType2tosState(
            MetadataAccessor::get_field_type_by_index(method->constants(), getfield_index()));
    Klass* field_holder_klass;
    if (is_static) {
        field_holder_klass = MetadataAccessor::get_field_holder_klass_if_loaded(method->constants(), getfield_index());
        if (field_holder_klass == NULL) {
            if (will_run) {
                field_holder_klass = resolve_field_return_klass(methodHandle(method), bs->bci(), JavaThread::current());
                __ movptr(c_obj, (intptr_t)field_holder_klass->java_mirror());
            } else {
                // patch
                PatchingStub *patchingStub = new PatchingStub(_masm, PatchingStub::load_mirror_id, bs->bci());
                __ movptr(c_obj, NULL_WORD);
                patchingStub->install();
                append_stub(patchingStub);
            }
        } else {
            __ movptr(c_obj, (intptr_t)field_holder_klass);
            __ ldr(c_obj, Address(c_obj, Klass::java_mirror_offset()));
        }
    }

    if (field_entry == NULL || !field_entry->is_resolved(bs->code())) {
        if (will_run) {
            Bytecode_field field_access(method, bs->bci());
            fieldDescriptor result; // initialize class if needed
            Bytecodes::Code code = field_access.code();
            constantPoolHandle constants(JavaThread::current(), method->constants());
            LinkResolver::resolve_field_access(result, constants, field_access.index(), Bytecodes::java_code(code), JavaThread::current());
            off = result.offset();
            return NULL;
        } else {
            // __ movptr(c_obj, (intptr_t)field_entry->f1_as_klass()->java_mirror());
            PatchingStub *patching = new PatchingStub(_masm, PatchingStub::access_field_id, bs->bci());
            // patch
            // __ movl(index, -1); // this value is patched later
            // later jmp dest is replaced to call_after
            return patching;
        }
    } else {
        off = field_entry->f2_as_index();
        return NULL;
    }
}

void NormalCompileTask::jsr_w() { Unimplemented(); }
void NormalCompileTask::goto_w() {
    branch(false, true, none);
}

void NormalCompileTask::invokevirtual(int byte_no) {
  transition(vtos, vtos);
  // how to do a invokevirtual call?
  // 1. resolve the function first
  // 2. compile the function, if not
  // 3. call the function
  // 4. put the function, jmp to / put the parameters
  // 5. the return handling of function

  // * need to change the calling convention, if necessary
  invoke(byte_no, rmethod, rmethod, r2, r3);
}

void NormalCompileTask::invokespecial(int byte_no) {
  transition(vtos, vtos);
  invoke(byte_no, rmethod, noreg, r2, noreg);
}

void NormalCompileTask::invokeinterface(int byte_no) {
  invoke(byte_no, rmethod, rmethod, r2, r3);
}

void NormalCompileTask::invokestatic(int byte_no) {
  transition(vtos, vtos);
  invoke(byte_no, rmethod, noreg, noreg, noreg);
}

void NormalCompileTask::invokedynamic(int byte_no) {
  D_WARN_Unimplement;
}

void NormalCompileTask::invoke(int byte_no, Register m, Register index, Register recv, Register flags) {
    // get the address of the function call, if not resolve, then return a patching

    gc_point();

     if (tos != vtos) {
         __ push(tos);
         tos = vtos;
     }

    int parameter_size = 0;
    const bool load_receiver    =   (recv   !=  noreg);
    // setup registers & access constant pool cache
    if (recv  == noreg)  recv  = r2;
    if (flags == noreg)  flags = r3;

    ConstantPoolCacheEntry *method_entry = method->constants()->cache()->entry_at(get_method_index());
    Method* callee = NULL;
    Klass* interface_klass;
    int vtable_index = 0;
    TosState called_tos = ilgl;
    address compiled_entry = NULL;
    PatchingStub* patch_compile = NULL;

    if (bs->code() == Bytecodes::_invokeinterface) {
      has_interface = true;
    }

    bool force_compile = bs->code() == Bytecodes::_invokeinterface;
    __ save_bcp();

    parameter_size = MetadataAccessor::method_parameter_size(method, bs->bci(), called_tos);
    if (load_receiver) {
        // patch the offset later
        parameter_size += 1;
        __ ldr(recv, Address(esp, (-1 + parameter_size) * Interpreter::stackElementSize));
    }

    // Note: method patching start from here
    // find the Method*
    // if the Method is not resolved, then resolve and compile it
    // else if the method is not compiled, then compile it later
    // else call the method directly
    if (method_entry == NULL || !method_entry->is_resolved(bs->code())) {
        // method is not loaded, do the patch and then compile it
        PatchingStub *stub = new PatchingStub(_masm, PatchingStub::load_method_id, bs->bci());
        Label final;
        if (bs->code() == Bytecodes::_invokeinterface) {
            __ movptr(r0, (long)this);
            __ movptr(index, (intptr_t)0xffff);
        } else if (bs->code() == Bytecodes::_invokevirtual) {
            // we do not know if this is a final method
            // we use rbx as both index and method register
            // if it is final, then mov method*, or else vtable_index
            // if it is not final, patch the jmp inst to jmp next inst

            // movl index_reg / method_reg, index / method*
            // jmp to after (patch as jmp to next if not final)
            // next: movptr rax, klass_off(recv)
            // movptr rbx, (index_reg * off + base)(rax)
            // after:
            __ movptr(m, (long)0);
            __ b(final);
        } else {
            compiled_entry = (address)-1;
            __ movptr(m, (long)0);
        }

        stub->install();
        append_stub(stub);

        if (bs->code() == Bytecodes::_invokeinterface) {
            __ load_klass(r3, recv);
            __ lookup_interface_method(// inputs: rec. class, interface, itable index
                    r3, r0, index,
                    // outputs: method, scan temp. reg
                    m, r13,
                    no_such_interface);
        } else if (bs->code() == Bytecodes::_invokevirtual) {
          __ load_klass(r0, recv);
          __ lookup_virtual_method(r0, index, m);
          __ bind(final);
        }

    } else {

        parameter_size = MetadataAccessor::method_parameter_size(method, bs->bci(), called_tos);

        if (load_receiver) {
            parameter_size += 1;
            __ ldr(recv, Address(esp, (-1 + parameter_size) * Interpreter::stackElementSize));
        }

        if (bs->code() == Bytecodes::_invokeinterface) {
            interface_klass = method_entry->f1_as_klass();
            vtable_index = method_entry->f2_as_index();

            if (method_entry->is_forced_virtual()) {
                    __ load_klass(r0, recv);
                    __ lookup_virtual_method(r0, vtable_index, m);
                    force_compile = false;
            } else {
                __ load_klass(r3, recv);
                __ ldr(r0, (Register)(intptr_t)interface_klass);

                __ lookup_interface_method(// inputs: rec. class, interface, itable index
                        r3, r0, vtable_index,
                        // outputs: method, scan temp. reg
                        m, r13,
                        no_such_interface);
            }
        } else {
            if (byte_no == f1_byte) {
                callee = method_entry->f1_as_method();
            } else {
                if (method_entry->is_vfinal()) {
                    callee = method_entry->f2_as_vfinal_method();
                } else {
                    vtable_index = method_entry->f2_as_index();
                    __ load_klass(r0, recv);
                    __ lookup_virtual_method(r0, vtable_index, m);
                }
            }

            if (callee) {
                if (will_run) {
                    EnclaveRuntime::compile_method(callee);
                }
                // load Method*, if invokestatic or vfinal
                __ movptr(m, (intptr_t)callee);
                if (JCompiler::is_compile(callee)) {
                    compiled_entry = callee->_from_compiled_entry;
                }
            }
        }
    }

    TosState tosState = method_entry->flag_state();

    // load appendix?
    switch (bs->code()) {
        case Bytecodes::_invokestatic: break; // do nothing
        // rbx is always Method*
        case Bytecodes::_invokespecial:
        case Bytecodes::_invokevirtual:
        case Bytecodes::_invokeinterface:
            __ null_check(recv);
            break;
        case Bytecodes::_invokehandle:
        case Bytecodes::_invokedynamic:
            Unimplemented();
            break;
        default:
            ShouldNotReachHere();
    }
    __ jump_to_compiled(m, compiled_entry, force_compile, patch_compile);

    // do not handle now
//    if (bs->code() == Bytecodes::_invokeinterface) {
//         __ bind(no_such_interface);
//         __ ldr(r0, (address)NULL);
//         __ b(r0);
//    }

    return_entry(tosState, parameter_size);
    tos = called_tos;
    if (patch_compile != NULL) {
        append_stub(patch_compile);
    }
}





void NormalCompileTask::checkcast() {
  transition(atos, atos);
  Label done, is_null, ok_is_subtype, quicked, resolved;
  __ cbz(r0, is_null);

  int idx = bs->get_index_u2();
  constantTag tag =  method->constants()->tag_at(idx);
  if (tag.is_klass()) {
    __ mov(r3, r0);
    __ movptr(r0, (intptr_t)method->constants()->klass_at_if_loaded(constantPoolHandle(method->constants()), idx));
  } else {
    __ mov(r3, r0);
    if (will_run) {
      Bytecode_checkcast cc(method, method->bcp_from(bs->bci()));
      Klass* klass = method->constants()->klass_at(cc.index(), JavaThread::current());
      __ movptr(r0, (intptr_t)klass);
    } else {
      PatchingStub *stub = new PatchingStub(_masm, PatchingStub::load_klass_id, bs->bci());
      __ movptr(r0, NULL_WORD);
      stub->install();
      append_stub(stub);
    }
  }

  // rax: klass
  // rdx: obj

  __ load_klass(r4, r3);

  // Generate subtype check.  Blows rcx, rdi.  Object in rdx.
  // Superklass in r0.  Subklass in r4.
  __ gen_subtype_check(r4, ok_is_subtype);

  // Come here on failure
  __ push_ptr(r3);
  // object is at TOS
  __ b(ExternalAddress(Interpreter::_throw_ClassCastException_entry));

  // Come here on success
  __ bind(ok_is_subtype);
  __ mov(r0, r3); // Restore object in rdx

  __ bind(is_null);   // same as 'done'
  __ bind(done);
}

void NormalCompileTask::instanceof() {
  transition(atos, itos);
  Label done, is_null, ok_is_subtype, quicked, resolved;
  __ cbz(r0, is_null);

  // rax: obj
  int idx = bs->get_index_u2();
  constantTag tag =  method->constants()->tag_at(idx);
  if (tag.is_klass()) {
    __ mov(r3, r0);
    __ movptr(r0, (intptr_t)method->constants()->klass_at_if_loaded(constantPoolHandle(method->constants()), idx));
  } else {
    __ mov(r3, r0);
    if (will_run) {
      Bytecode_instanceof io(method, method->bcp_from(bs->bci()));
      Klass* k = method->constants()->klass_at(io.index(), JavaThread::current());
      __ movptr(r0, (intptr_t)k);
    } else {
      PatchingStub *stub = new PatchingStub(_masm, PatchingStub::load_klass_id, bs->bci());
      __ movptr(r0, NULL_WORD);
      stub->install();
      append_stub(stub);
    }
  }
  __ load_klass(r3, r3);
  // r3: klass of obj
  // r0: klass of instanceof klass


  // Generate subtype check.  Blows rcx, rdi
  // Superklass in rax.  Subklass in rdx.
  __ gen_subtype_check(r3, ok_is_subtype);

  // Come here on failure
  __ mov(r0, 0);
  __ b(done);
  // Come here on success
  __ bind(ok_is_subtype);
  __ mov(r0, 1);

  // Collect counts on whether this test sees NULLs a lot or not.
  __ bind(is_null);   // same as 'done'
  __ bind(done);
  // r0 = 0: obj == NULL or  obj is not an instanceof the specified klass
  // r0 = 1: obj != NULL and obj is     an instanceof the specified klass
}

//need to add to _new, invoke, etc according to /openjdk-sgx/hotspot/enclave_src/share/vm/c0/NormalCompileTask.cpp
void NormalCompileTask::gc_point() {
    //temporarily comment out
    //__ movptr(Address(rmethod, frame::interpreter_frame_last_sp_offset * wordSize), bs->bci());
    oopSet->put_entry(bs->bci(), __ current_entry->clone());
}

void NormalCompileTask::lookupswitch() {
  transition(itos, itos);
  // bytecode structure of tableswitch
  // u1 opcode
  // 0 - 3 byte padding
  // default_offset
  // n
  // <high>
  Label default_case;
  // the val is stored in rax
  intptr_t offset_start = round_to(bs->bcp() + 1, 4);
  int32_t* offsets = (int32_t*)(offset_start);

  int branch_n = Bytes::get_Java_u4((address)(offsets + 1));
  int default_bci_offset = Bytes::get_Java_u4((address)(offsets + 0));
  int target;
  int target_key;
  for (int i = 0;i < branch_n;i++) {
    target_key = Bytes::get_Java_u4((address)(offsets + i * 2 + 2));
    target = bs->bci() + Bytes::get_Java_u4((address)(offsets + i * 2 + 2 + 1));
    __ cmpw(r0, target_key);
    jump_target(target, equal);
  }
  jump_target(bs->bci() + default_bci_offset, none);
}

void NormalCompileTask::tableswitch() {
  // bytecode structure of tableswitch
  // u1 opcode
  // 0 - 3 byte padding
  // default_offset
  // <low>
  // <high>

  Label default_case;
  // the val is stored in rax

  intptr_t offset_start = round_to(bs->bcp() + 1, 4);
  uint32_t* offsets = (uint32_t*)(offset_start);

  int low = Bytes::get_Java_u4((address)(offsets + 1));
  int high = Bytes::get_Java_u4((address)(offsets + 2));
  int default_bci_offset = Bytes::get_Java_u4((address)(offsets + 0));

  // compare low
  __ cmpw(r0, low);
  jump_target(bs->bci() + default_bci_offset, less);
  // compare high
  __ cmpw(r0, high);
  jump_target(bs->bci() + default_bci_offset, greater);
  __ sub(r0, r0, low);

  // store the dispatch table
  address mov_dispatch_table = __ pc();
  __ movptr(r3, NULL);
  __ ldr(r0, Address(r3, r0, Address::uxtw(2)));
  __ b(r0);
  address dispatch_table_start = __ pc();
  NativeMovConstReg* mov_dispatch = nativeMovConstReg_at(mov_dispatch_table);
  mov_dispatch->set_data((intptr_t)dispatch_table_start);

  int target;
  for (int i = 0;i < high - low + 1;i++) {
    target = bs->bci() + Bytes::get_Java_u4((address)(offsets + i + 3));
    patch_dispatch.push_back(std::pair<int, address> (target, __ pc()));
    __ emit_address(NULL);
    jmp_target.insert(std::pair<int, TosState>(target, vtos));
  }
}

void NormalCompileTask::jump_target(int target, Condition cc) {

    TosState after_tos;
    if (bs->code() == Bytecodes::_goto) {
        after_tos = tos;
    } else {
        after_tos = vtos;
    }

    // adjust the tos as required
    std::map<int, TosState>::iterator find_target = jmp_target.find(target);
    if (find_target != jmp_target.end()) {
        // find it, cast it to the target
        TosState target_tos = find_target->second;
        if (target_tos == vtos && after_tos != vtos) {
            __ push(after_tos);
        } else if (target_tos != vtos && after_tos == vtos) {
            __ pop(target_tos);
        } else {
            checkcast_state(target_tos, after_tos);
        }
    } else {
        jmp_target.insert(std::pair<int, TosState>(target, after_tos));
    }

    std::map<int, address>::iterator itr = bci_ptr_map.find(target);
    if (itr != bci_ptr_map.end()) {
        address next_addr = __ pc() + NativeGeneralJump::instruction_size;
        //no none condition
        if (cc == none) {
            __ b(itr->second);
        } else {
            next_addr += 1;
            D_WARN_Unimplement;
        }
    } else {
        patch_address.push_back(std::pair<int, address>(target, __ pc()));
        //no none condition
        if (cc == none) {
            __ b(__ pc() + NativeGeneralJump::instruction_size);
        } else {
          D_WARN_Unimplement;
        }
    }

}

void NormalCompileTask::remove_activation(TosState state, Register ret_addr, bool throw_monitor_exception,
                                          bool install_monitor_exception, bool notify_jvmdi) {

    // remove activation
    // movptr change to str
    // get sender sp

    __ ldr(esp,
           Address(rfp, frame::interpreter_frame_sender_sp_offset * wordSize));
    __ leave();                           // remove frame anchor
    __ andr(sp, esp, -16);                // set sp to sender sp
}



void NormalCompileTask::narrow(Register result, TosState tos) {
    switch (tos) {
        case ztos: __ andw(result, result, 0x1);    break;
        case stos: __ sbfx(result, result, 0, 16);  break;
        case btos: __ sbfx(result, result, 0, 8);   break;
        case ctos: __ ubfx(result, result, 0, 16);  break;
        default:                                    break;
    }
}

//not found in templatetable aarch64
void NormalCompileTask::_jmp_return() {
    //jmp change to b
    __ b(ret_now);
    ret_tos = tos;
    tos = udtos;
    will_run = false;
}


void NormalCompileTask::adjust_tos() {
    TosState pre_state = tos;
    TosState find_state;
    // if tos is udtos, then the current bci should be jump from other source
    std::map<int, TosState>::iterator it = jmp_target.find(bs->bci());
    find_state = (it != jmp_target.end())? it->second : udtos;
    if (find_state == udtos) {
        tos = pre_state;
    } else {
        if (pre_state == udtos) {
            // no potential conflicts
            tos = find_state;
        } else {
            // both are determined, we should follow find_state,
            if (find_state == vtos && pre_state != vtos) {
                __ push(pre_state);
            } else if (find_state != vtos && pre_state == vtos) {
                __ pop(find_state);
            } else {
                // check if can be cast
                checkcast_state(find_state, pre_state);
            }
            tos = find_state;
        }
    }

    // if tos is still udtos, then change it to udtos, this happen when both are not found
    if (tos == udtos) {
      // D_COMPILE("can not find the target tos %s:%s:%d\n", method->klass_name()->as_C_string(), method->name()->as_C_string(), bs->bci());
      tos = vtos;
    }
}

//-----------------------------------------------------------------------------
// Exceptions

void NormalCompileTask::athrow() {
  transition(atos, vtos);
  __ null_check(r0);
  __ b(Interpreter::throw_exception_entry());
}
//-----------------------------------------------------------------------------
// Synchronization
//
// Note: monitorenter & exit are symmetric routines; which is reflected
//       in the assembly code structure as well
//
// Stack layout:
//
// [expressions  ] <--- esp               = expression stack top
// ..
// [expressions  ]
// [monitor entry] <--- monitor block top = expression stack bot
// ..
// [monitor entry]
// [frame data   ] <--- monitor block bot
// ...
// [saved rbp    ] <--- rbp

 //comment out
void NormalCompileTask::monitorenter() {
  D_WARN_Unimplement;
}
//comment out
void NormalCompileTask::monitorexit() {
  D_WARN_Unimplement;
}

inline bool is_jump_code(Bytecodes::Code code) {
    return ((code >= Bytecodes::_lcmp && code <= Bytecodes::_lookupswitch)
            || (code >= Bytecodes::_ifnull && code <= Bytecodes::_breakpoint));
}

inline bool is_invoke_code(Bytecodes::Code code) {
    return (code >= Bytecodes::_invokespecial && code <= Bytecodes::_invokedynamic);
}

inline bool is_klass_code(Bytecodes::Code code) {
    return (code >= Bytecodes::_new && code <= Bytecodes::_multianewarray);
}

#undef __
