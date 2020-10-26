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
#include <nativeInst_x86.hpp>
#include <runtime/fieldType.hpp>
#include <enclave/EnclaveNative.h>
#include "interpreter/interpreter.hpp"
#include "c0/c0_NormalCompileTask.hpp"
#include "c0/bytecodeStream.hpp"
#include "c0/c0_CodeStubs.hpp"
#include "c0/MetadataAccessor.hpp"
#include "c0/c0_Runtime.hpp"
#include "c0_Disassembler_x86.hpp"

#include <interpreter/interpreterRuntime.hpp>
#define __ _masm->

// 1. If we expect vtos, push rax into stacks
// 2. If we expect non-tos, pop to rax
// 3. If we have (vtos, vtos), the tos is decided in the generator

extern int is_sgx_interface(const Method* m);

extern Klass* resolve_field_return_klass(methodHandle caller, int bci, TRAPS);

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

static inline Address iaddress(int n) {
    return Address(r14, Interpreter::local_offset_in_bytes(n));
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
    return Address(r14, r, Address::times_8);
}

static inline Address laddress(Register r) {
    return Address(r14, r, Address::times_8, Interpreter::local_offset_in_bytes(1));
}

static inline Address faddress(Register r) {
    return iaddress(r);
}

static inline Address daddress(Register r) {
    return laddress(r);
}

static inline Address aaddress(Register r) {
    return iaddress(r);
}

static inline Address at_rsp() {
    return Address(rsp, 0);
}

static inline Address at_tos   () {
    return Address(rsp,  Interpreter::expr_offset_in_bytes(0));
}

static inline Address at_tos_p1() {
    return Address(rsp,  Interpreter::expr_offset_in_bytes(1));
}

static inline Address at_tos_p2() {
    return Address(rsp,  Interpreter::expr_offset_in_bytes(2));
}

static inline Address at_tos_p3() {
    return Address(rsp,  Interpreter::expr_offset_in_bytes(3));
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

void NormalCompileTask::nop() {}
void NormalCompileTask::aconst_null() { __ xorl(rax, rax); }
void NormalCompileTask::iconst(int value) {
    if (value == 0) {
        __ xorl(rax, rax);
    }
    else {
        __ movl(rax, value);
    }
}
void NormalCompileTask::lconst(int value) {
    if (value == 0) {
        __ xorl(rax, rax);
    }
    else {
        __ movl(rax, value);
    }
}
void NormalCompileTask::fconst(int value) {
    static float one = 1.0f, two = 2.0f;
    switch (value) {
        case 0:
            __ xorps(xmm0, xmm0);
            break;
        case 1:
            __ movflt(xmm0, ExternalAddress((address) &one));
            break;
        case 2:
            __ movflt(xmm0, ExternalAddress((address) &two));
            break;
        default:
            ShouldNotReachHere();
            break;
    }
}
void NormalCompileTask::dconst(int value) {
    static double one = 1.0;
    switch (value) {
        case 0:
            __ xorpd(xmm0, xmm0);
            break;
        case 1:
            __ movdbl(xmm0, ExternalAddress((address) &one));
            break;
        default:
            ShouldNotReachHere();
            break;
    }
}
void NormalCompileTask::bipush() {
    __ movl(rax, bs->get_index_signed_1());
}
void NormalCompileTask::sipush() {
    uint16_t uvalue = bs->get_index_u2();
    int16_t* signed_value_pointer = (int16_t*)&uvalue;
    __ movl(rax, *signed_value_pointer);
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
        Klass* resolve_klass = NULL;
        if (Bytecodes::uses_cp_cache(bs->raw_code())) {
            resolve_constant = method->constants()->resolve_cached_constant_at(-cache_idx, JavaThread::current());
        }
        if (resolve_constant == NULL) {
            if (will_run) {
                Bytecode_loadconstant bytecodeLoadconstant(methodHandle(method), bs->bci());
                resolve_constant = bytecodeLoadconstant.resolve_constant(JavaThread::current());
                resolve_klass = java_lang_Class::as_Klass(resolve_constant);
                __ movptr(rax, (intptr_t)resolve_klass);
            } else {
                PatchingStub *patchingStub = new PatchingStub(_masm, PatchingStub::load_mirror_id, bs->bci());
                __ movptr(rax, NULL_WORD);
                patchingStub->install();
                append_stub(patchingStub);
            }
            __ movptr(rax, Address(rax, Klass::java_mirror_offset()));
        } else {
            resolve_klass = java_lang_Class::as_Klass(resolve_constant);
            __ movptr(rax, (intptr_t)resolve_klass);
            __ movptr(rax, Address(rax, Klass::java_mirror_offset()));
        }
        tos = atos;
    } else if (tag.is_string()) {
        // call ldc
        oop resolve_constant = NULL;
        oop enclave_constant = NULL;
        if (Bytecodes::uses_cp_cache(bs->raw_code())) {
            resolve_constant = method->constants()->resolve_cached_constant_at(-cache_idx, JavaThread::current());
        }
        if (resolve_constant == NULL) {
            if (will_run) {
                Bytecode_loadconstant bytecodeLoadconstant(methodHandle(method), bs->bci());
                resolve_constant = bytecodeLoadconstant.resolve_constant(JavaThread::current());
                enclave_constant = StringTable::intern(resolve_constant, JavaThread::current());
                __ movptr(rax, (intptr_t)enclave_constant);
            } else {
                PatchingStub *patchingStub = new PatchingStub(_masm, PatchingStub::load_mirror_id, bs->bci());
                __ movptr(rax, NULL_WORD);
                patchingStub->install();
                append_stub(patchingStub);
            }
        } else {
            enclave_constant = StringTable::intern(resolve_constant, JavaThread::current());
            __ movptr(rax, (intptr_t)enclave_constant);
        }
        tos = atos;
    } else {
        if (tag.is_float()) {
            // ftos
            __ movflt(xmm0, ExternalAddress((address)method->constants()->float_at_addr(idx)) );
//            __ push_f();
            tos = ftos;
        } else {
            // itos JVM_CONSTANT_Integer only
            __ movl(rax, method->constants()->int_at(idx));
//            __ push_i(rax);
            tos = itos;
        }
    }

}
void NormalCompileTask::ldc2_w() {
    if (tos != vtos) {
        __ push(tos);
    }

    int idx = bs->get_index_u2();
    constantTag tag = method->constants()->tag_at(idx);
    if (tag.is_double()) {
        __ movptr(rcx, (intptr_t)method->constants()->double_at_addr(idx));
        __ movdbl(xmm0, Address(rcx, 0));
        tos = dtos;
    } else if (tag.is_long()) {
        __ movptr(rax, (intptr_t)method->constants()->long_at_addr(idx));
        __ movq(rax, Address(rax, 0));
        tos = ltos;
    } else {
        ShouldNotReachHere();
    }
}
void NormalCompileTask::iload() {
    __ movl(rax, iaddress(bs->get_index_u1()));
}
void NormalCompileTask::iload(int n) {
    __ movl(rax, iaddress(n));
}
void NormalCompileTask::lload() {
    __ movq(rax, laddress(bs->get_index_u1()));
}
void NormalCompileTask::lload(int n) {
    __ movq(rax, laddress(n));
}
void NormalCompileTask::fload() {
    __ movflt(xmm0, faddress(bs->get_index_u1()));
}
void NormalCompileTask::fload(int n) {
    __ movflt(xmm0, faddress(n));
}
void NormalCompileTask::dload() {
    __ movdbl(xmm0, daddress(bs->get_index_u1()));
}
void NormalCompileTask::dload(int n) {
    __ movdbl(xmm0, daddress(n));
}
void NormalCompileTask::aload() {
    __ movptr(rax, aaddress(bs->get_index_u1()));
}
void NormalCompileTask::aload(int n) {
    __ movptr(rax, aaddress(n));
}
void NormalCompileTask::aload_0() {
    aload(0);
}
void NormalCompileTask::iaload() {
    __ pop_ptr(rdx);
    // eax: index
    // rdx: array
    index_check(rdx, rax); // kills rbx
    __ movl(rax, Address(rdx, rax,
                         Address::times_4,
                         arrayOopDesc::base_offset_in_bytes(T_INT)));
}
void NormalCompileTask::laload() {
    __ pop_ptr(rdx);
    // eax: index
    // rdx: array
    index_check(rdx, rax); // kills rbx
    __ movq(rax, Address(rdx, rbx,
                         Address::times_8,
                         arrayOopDesc::base_offset_in_bytes(T_LONG)));
}
void NormalCompileTask::faload() {
    __ pop_ptr(rdx);
    // eax: index
    // rdx: array
    index_check(rdx, rax); // kills rbx
    __ movflt(xmm0, Address(rdx, rax,
                            Address::times_4,
                            arrayOopDesc::base_offset_in_bytes(T_FLOAT)));
}
void NormalCompileTask::daload() {
    __ pop_ptr(rdx);
    // eax: index
    // rdx: array
    index_check(rdx, rax); // kills rbx
    __ movdbl(xmm0, Address(rdx, rax,
                            Address::times_8,
                            arrayOopDesc::base_offset_in_bytes(T_DOUBLE)));
}
void NormalCompileTask::aaload() {
    __ pop_ptr(rdx);
    // eax: index
    // rdx: array
    index_check(rdx, rax); // kills rbx
    __ load_heap_oop(rax, Address(rdx, rax,
                                  UseCompressedOops ? Address::times_4 : Address::times_8,
                                  arrayOopDesc::base_offset_in_bytes(T_OBJECT)));
}
void NormalCompileTask::baload() {
    __ pop_ptr(rdx);
    // eax: index
    // rdx: array
    index_check(rdx, rax); // kills rbx
    __ load_signed_byte(rax,
                        Address(rdx, rax,
                                Address::times_1,
                                arrayOopDesc::base_offset_in_bytes(T_BYTE)));
}
void NormalCompileTask::caload() {
    __ pop_ptr(rdx);
    // eax: index
    // rdx: array
    index_check(rdx, rax); // kills rbx
    __ load_unsigned_short(rax,
                           Address(rdx, rax,
                                   Address::times_2,
                                   arrayOopDesc::base_offset_in_bytes(T_CHAR)));
}
void NormalCompileTask::saload() {
    __ pop_ptr(rdx);
    // eax: index
    // rdx: array
    index_check(rdx, rax); // kills rbx
    __ load_signed_short(rax,
                         Address(rdx, rax,
                                 Address::times_2,
                                 arrayOopDesc::base_offset_in_bytes(T_SHORT)));
}
void NormalCompileTask::istore(){
    __ movl(iaddress(bs->get_index_u1()), rax);
}
void NormalCompileTask::istore(int n){
    __ movl(iaddress(n), rax);
}
void NormalCompileTask::lstore(){
    __ movq(laddress(bs->get_index_u1()), rax);
}
void NormalCompileTask::lstore(int n){
    __ movq(laddress(n), rax);
}
void NormalCompileTask::fstore(){
    __ movflt(faddress(bs->get_index_u1()), xmm0);
}
void NormalCompileTask::fstore(int n){
    __ movflt(faddress(n), xmm0);
}
void NormalCompileTask::dstore(){
    __ movdbl(daddress(bs->get_index_u1()), xmm0);
}
void NormalCompileTask::dstore(int n){
    __ movdbl(daddress(n), xmm0);
}
void NormalCompileTask::astore(){
    __ current_entry->set_bit(bs->get_index_u1(), C0MapOopEntry::oop_bit_number);
    __ movptr(aaddress(bs->get_index_u1()), rax);
}
void NormalCompileTask::astore(int n){
    __ current_entry->set_bit(n, C0MapOopEntry::oop_bit_number);
    __ movptr(aaddress(n), rax);
}
void NormalCompileTask::iastore() {
    __ pop_i(rbx);
    __ pop_ptr(rdx);
    // eax: value
    // ebx: index
    // rdx: array
    index_check(rdx, rbx); // prefer index in ebx
    __ movl(Address(rdx, rbx,
                    Address::times_4,
                    arrayOopDesc::base_offset_in_bytes(T_INT)),
            rax);
}
void NormalCompileTask::lastore() {
    __ pop_i(rbx);
    __ pop_ptr(rdx);
    // rax: value
    // ebx: index
    // rdx: array
    index_check(rdx, rbx); // prefer index in ebx
    __ movq(Address(rdx, rbx,
                    Address::times_8,
                    arrayOopDesc::base_offset_in_bytes(T_LONG)),
            rax);
}
void NormalCompileTask::fastore() {
    __ pop_i(rbx);
    __ pop_ptr(rdx);
    // xmm0: value
    // ebx:  index
    // rdx:  array
    index_check(rdx, rbx); // prefer index in ebx
    __ movflt(Address(rdx, rbx,
                      Address::times_4,
                      arrayOopDesc::base_offset_in_bytes(T_FLOAT)),
              xmm0);
}
void NormalCompileTask::dastore() {
    __ pop_i(rbx);
    __ pop_ptr(rdx);
    // xmm0: value
    // ebx:  index
    // rdx:  array
    index_check(rdx, rbx); // prefer index in ebx
    __ movdbl(Address(rdx, rbx,
                      Address::times_8,
                      arrayOopDesc::base_offset_in_bytes(T_DOUBLE)),
              xmm0);
}
void NormalCompileTask::aastore() {
    if (tos != vtos) {
        __ push(tos);
        tos = vtos;
    }
    Label is_null, ok_is_subtype, done;

    // stack: ..., array, index, value
    // tos in rax
    __ movptr(rax, at_tos());    // value
    __ movl(rcx, at_tos_p1()); // index
    __ movptr(rdx, at_tos_p2()); // array

    Address element_address(rdx, rcx,
                            UseCompressedOops? Address::times_4 : Address::times_8,
                            arrayOopDesc::base_offset_in_bytes(T_OBJECT));

    index_check(rdx, rcx);     // kills rbx
    // do array store check - check for NULL value first
    __ testptr(rax, rax);
    __ jcc(Assembler::zero, is_null);

    // Move subklass into rbx
    __ load_klass(rbx, rax);
    // Move superklass into rax
    __ load_klass(rax, rdx);
    __ movptr(rax, Address(rax,
                           ObjArrayKlass::element_klass_offset()));
    // Compress array + index*oopSize + 12 into a single register.  Frees rcx.
    __ lea(rdx, element_address);

    // Generate subtype check.  Blows rcx, rdi
    // Superklass in rax.  Subklass in rbx.
    __ gen_subtype_check(rbx, ok_is_subtype);

    // Come here on failure
    // object is at TOS
    __ jump(ExternalAddress(Interpreter::_throw_ArrayStoreException_entry));

    // Come here on success
    __ bind(ok_is_subtype);

    // Get the value we will store
    __ movptr(rax, at_tos());
    // Now store using the appropriate barrier
    __ store_heap_oop(Address(rdx, 0), rax);
    __ jmp(done);

    // Have a NULL in rax, rdx=array, ecx=index.  Store NULL at ary[idx]
    __ bind(is_null);
    __ profile_null_seen(rbx);

    // Store a NULL
    __ store_heap_oop_null(element_address);
    // Pop stack arguments
    __ bind(done);
    __ addptr(rsp, 3 * Interpreter::stackElementSize);
}
void NormalCompileTask::bastore() {
    __ pop_i(rbx);
    __ pop_ptr(rdx);
    // eax: value
    // ebx: index
    // rdx: array
    index_check(rdx, rbx); // prefer index in ebx
    // Need to check whether array is boolean or byte
    // since both types share the bastore bytecode.
    __ load_klass(rcx, rdx);
    __ movl(rcx, Address(rcx, Klass::layout_helper_offset()));
    int diffbit = Klass::layout_helper_boolean_diffbit();
    __ testl(rcx, diffbit);
    Label L_skip;
    __ jccb(Assembler::zero, L_skip);
    __ andl(rax, 1);  // if it is a T_BOOLEAN array, mask the stored value to 0/1
    __ bind(L_skip);
    __ movb(Address(rdx, rbx,
                    Address::times_1,
                    arrayOopDesc::base_offset_in_bytes(T_BYTE)),
            rax);
}
void NormalCompileTask::castore() {
    __ pop_i(rbx);
    __ pop_ptr(rdx);
    // eax: value
    // ebx: index
    // rdx: array
    index_check(rdx, rbx);  // prefer index in ebx
    __ movw(Address(rdx, rbx,
                    Address::times_2,
                    arrayOopDesc::base_offset_in_bytes(T_CHAR)),
            rax);
}
void NormalCompileTask::sastore() {
    castore();
}
void NormalCompileTask::pop() {
    if (tos == vtos)
        __ addptr(rsp, Interpreter::stackElementSize);
    tos = vtos;
}
void NormalCompileTask::pop2() {
    if (tos == vtos)
        __ addptr(rsp, 2 * Interpreter::stackElementSize);
    else
        __ addptr(rsp, 1 * Interpreter::stackElementSize);

}
void NormalCompileTask::dup() {
    if (tos == vtos) {
        __ load_ptr(0, rax);
        tos = atos;
    }
    else
      __ push_ptr(rax);
}
void NormalCompileTask::dup_x1() {
    if (tos == vtos) {
        __ load_ptr(0, rax);  // load b
        __ load_ptr(1, rcx);  // load a
        __ store_ptr(1, rax);  // store b
        __ store_ptr(0, rcx);  // store a
        __ push_ptr(rax);      // push b
    } else {
        __ load_ptr(0, rcx);
        __ store_ptr(0, rax);
        __ push_ptr(rcx);
    }
}
void NormalCompileTask::dup_x2() {
    if (tos == vtos) {
        // stack: ..., a, b, c
        __ load_ptr(0, rax);  // load c
        __ load_ptr(2, rcx);  // load a
        __ store_ptr(2, rax);  // store c in a
        __ push_ptr(rax);      // push c
        // stack: ..., c, b, c, c
        __ load_ptr(2, rax);  // load b
        __ store_ptr(2, rcx);  // store a in b
        // stack: ..., c, a, c, c
        __ store_ptr(1, rax);  // store b in c
        // stack: ..., c, a, b, c
    } else {
        __ load_ptr(0, rbx);
        __ load_ptr(1, rcx);
        __ push_ptr(rbx);
        __ store_ptr(2, rax);
        __ store_ptr(1, rcx);
        __ push_ptr(rbx);
    }
}
void NormalCompileTask::dup2() {
    if (tos == vtos) {
        // stack: ..., a, b
        __ load_ptr(1, rax);  // load a
        __ push_ptr(rax);     // push a
        __ load_ptr(1, rax);  // load b
        __ push_ptr(rax);     // push b
        // stack: ..., a, b, a, b
    } else if (tos == ltos || tos == dtos) {
        // tos is two-word length
        __ push(tos);
    } else {
        __ load_ptr(0, rcx);
        __ push_ptr(rax);
        __ push_ptr(rcx);
    }
}
void NormalCompileTask::dup2_x1(){
    if (tos != vtos) {
        __ push(tos);
        tos = vtos;
    }
    // stack: ..., a, b, c
    __ load_ptr( 0, rcx);  // load c
    __ load_ptr( 1, rax);  // load b
    __ push_ptr(rax);      // push b
    __ push_ptr(rcx);      // push c
    // stack: ..., a, b, c, b, c
    __ store_ptr(3, rcx);  // store c in b
    // stack: ..., a, c, c, b, c
    __ load_ptr( 4, rcx);  // load a
    __ store_ptr(2, rcx);  // store a in 2nd c
    // stack: ..., a, c, a, b, c
    __ store_ptr(4, rax);  // store b in a
    // stack: ..., b, c, a, b, c
}
void NormalCompileTask::dup2_x2(){
    if (tos != vtos) {
        __ push(tos);
        tos = vtos;
    }
    // stack: ..., a, b, c, d
    __ load_ptr( 0, rcx);  // load d
    __ load_ptr( 1, rax);  // load c
    __ push_ptr(rax);      // push c
    __ push_ptr(rcx);      // push d
    // stack: ..., a, b, c, d, c, d
    __ load_ptr( 4, rax);  // load b
    __ store_ptr(2, rax);  // store b in d
    __ store_ptr(4, rcx);  // store d in b
    // stack: ..., a, d, c, b, c, d
    __ load_ptr( 5, rcx);  // load a
    __ load_ptr( 3, rax);  // load c
    __ store_ptr(3, rcx);  // store a in c
    __ store_ptr(5, rax);  // store c in a
    // stack: ..., c, d, a, b, c, d
}
void NormalCompileTask::swap(){
    if (tos == vtos) {
        // stack: ..., a, b
        __ load_ptr(1, rcx);  // load a
        __ load_ptr(0, rax);  // load b
        __ store_ptr(0, rcx);  // store a in b
        __ store_ptr(1, rax);  // store b in a
        // stack: ..., b, a
    } else {
        __ load_ptr(0, rcx);
        __ store_ptr(0, rax);
        __ mov(rax, rcx);
    }
}
void NormalCompileTask::iop2(Operation op){
    switch (op) {
        case add  :                    __ pop_i(rdx); __ addl (rax, rdx); break;
        case sub  : __ movl(rdx, rax); __ pop_i(rax); __ subl (rax, rdx); break;
        case mul  :                    __ pop_i(rdx); __ imull(rax, rdx); break;
        case _and :                    __ pop_i(rdx); __ andl (rax, rdx); break;
        case _or  :                    __ pop_i(rdx); __ orl  (rax, rdx); break;
        case _xor :                    __ pop_i(rdx); __ xorl (rax, rdx); break;
        case shl  : __ movl(rcx, rax); __ pop_i(rax); __ shll (rax);      break;
        case shr  : __ movl(rcx, rax); __ pop_i(rax); __ sarl (rax);      break;
        case ushr : __ movl(rcx, rax); __ pop_i(rax); __ shrl (rax);      break;
        default   : ShouldNotReachHere();
    }
}
void NormalCompileTask::lop2(Operation op){
    switch (op) {
        case add  :                    __ pop_l(rdx); __ addptr(rax, rdx); break;
        case sub  : __ mov(rdx, rax);  __ pop_l(rax); __ subptr(rax, rdx); break;
        case _and :                    __ pop_l(rdx); __ andptr(rax, rdx); break;
        case _or  :                    __ pop_l(rdx); __ orptr (rax, rdx); break;
        case _xor :                    __ pop_l(rdx); __ xorptr(rax, rdx); break;
        default   : ShouldNotReachHere();
    }
}
void NormalCompileTask::fop2(Operation op){
    switch (op) {
        case add:
            __ addss(xmm0, at_rsp());
            __ addptr(rsp, Interpreter::stackElementSize);
            break;
        case sub:
            __ movflt(xmm1, xmm0);
            __ pop_f(xmm0);
            __ subss(xmm0, xmm1);
            break;
        case mul:
            __ mulss(xmm0, at_rsp());
            __ addptr(rsp, Interpreter::stackElementSize);
            break;
        case div:
            __ movflt(xmm1, xmm0);
            __ pop_f(xmm0);
            __ divss(xmm0, xmm1);
            break;
        case rem:
            __ movflt(xmm1, xmm0);
            __ pop_f(xmm0);
            __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::frem), 2);
            break;
        default:
            ShouldNotReachHere();
            break;
    }
}
void NormalCompileTask::dop2(Operation op){
    switch (op) {
        case add:
            __ addsd(xmm0, at_rsp());
            __ addptr(rsp, 2 * Interpreter::stackElementSize);
            break;
        case sub:
            __ movdbl(xmm1, xmm0);
            __ pop_d(xmm0);
            __ subsd(xmm0, xmm1);
            break;
        case mul:
            __ mulsd(xmm0, at_rsp());
            __ addptr(rsp, 2 * Interpreter::stackElementSize);
            break;
        case div:
            __ movdbl(xmm1, xmm0);
            __ pop_d(xmm0);
            __ divsd(xmm0, xmm1);
            break;
        case rem:
            __ movdbl(xmm1, xmm0);
            __ pop_d(xmm0);
            __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::drem), 2);
            break;
        default:
            ShouldNotReachHere();
            break;
    }
}
void NormalCompileTask::irem(){
    __ movl(rcx, rax);
    __ pop_i(rax);
    // Note: could xor eax and ecx and compare with (-1 ^ min_int). If
    //       they are not equal, one could do a normal division (no correction
    //       needed), which may speed up this implementation for the common case.
    //       (see also JVM spec., p.243 & p.271)
    __ corrected_idivl(rcx);
    __ movl(rax, rdx);
}
void NormalCompileTask::lrem(){
    __ mov(rcx, rax);
    __ pop_l(rax);
    __ testq(rcx, rcx);
    __ jump_cc(Assembler::zero,
               ExternalAddress(Interpreter::_throw_ArithmeticException_entry));
    // Note: could xor rax and rcx and compare with (-1 ^ min_int). If
    //       they are not equal, one could do a normal division (no correction
    //       needed), which may speed up this implementation for the common case.
    //       (see also JVM spec., p.243 & p.271)
    __ corrected_idivq(rcx); // kills rbx
    __ mov(rax, rdx);
}
void NormalCompileTask::ineg(){
    __ negl(rax);
}
void NormalCompileTask::lneg(){
    __ negq(rax);
}
void NormalCompileTask::fneg(){
    static jlong *float_signflip  = double_quadword(&float_signflip_pool[1], 0x8000000080000000, 0x8000000080000000);
    __ xorps(xmm0, ExternalAddress((address) float_signflip));
}
void NormalCompileTask::dneg(){
    static jlong *double_signflip  = double_quadword(&double_signflip_pool[1], 0x8000000000000000, 0x8000000000000000);
    __ xorpd(xmm0, ExternalAddress((address) double_signflip));
}
void NormalCompileTask::lshl(){
    __ movl(rcx, rax);                             // get shift count
    __ pop_l(rax);                                 // get shift value
    __ shlq(rax);
}
void NormalCompileTask::lshr(){
    __ movl(rcx, rax);                             // get shift count
    __ pop_l(rax);                                 // get shift value
    __ sarq(rax);
}
void NormalCompileTask::lushr(){
    __ movl(rcx, rax);                             // get shift count
    __ pop_l(rax);                                 // get shift value
    __ shrq(rax);
}
void NormalCompileTask::convert(){
    static const int64_t is_nan = 0x8000000000000000L;

    // Conversion
    switch (bs->code()) {
        case Bytecodes::_i2l:
            __ movslq(rax, rax);
            break;
        case Bytecodes::_i2f:
            __ cvtsi2ssl(xmm0, rax);
            break;
        case Bytecodes::_i2d:
            __ cvtsi2sdl(xmm0, rax);
            break;
        case Bytecodes::_i2b:
            __ movsbl(rax, rax);
            break;
        case Bytecodes::_i2c:
            __ movzwl(rax, rax);
            break;
        case Bytecodes::_i2s:
            __ movswl(rax, rax);
            break;
        case Bytecodes::_l2i:
            __ movl(rax, rax);
            break;
        case Bytecodes::_l2f:
            __ cvtsi2ssq(xmm0, rax);
            break;
        case Bytecodes::_l2d:
            __ cvtsi2sdq(xmm0, rax);
            break;
        case Bytecodes::_f2i:
        {
            Label L;
            __ cvttss2sil(rax, xmm0);
            __ cmpl(rax, 0x80000000); // NaN or overflow/underflow?
            __ jcc(Assembler::notEqual, L);
            __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::f2i), 1);
            __ bind(L);
        }
            break;
        case Bytecodes::_f2l:
        {
            Label L;
            __ cvttss2siq(rax, xmm0);
            // NaN or overflow/underflow?
            __ cmp64(rax, ExternalAddress((address) &is_nan));
            __ jcc(Assembler::notEqual, L);
            __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::f2l), 1);
            __ bind(L);
        }
            break;
        case Bytecodes::_f2d:
            __ cvtss2sd(xmm0, xmm0);
            break;
        case Bytecodes::_d2i:
        {
            Label L;
            __ cvttsd2sil(rax, xmm0);
            __ cmpl(rax, 0x80000000); // NaN or overflow/underflow?
            __ jcc(Assembler::notEqual, L);
            __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::d2i), 1);
            __ bind(L);
        }
            break;
        case Bytecodes::_d2l:
        {
            Label L;
            __ cvttsd2siq(rax, xmm0);
            // NaN or overflow/underflow?
            __ cmp64(rax, ExternalAddress((address) &is_nan));
            __ jcc(Assembler::notEqual, L);
            __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::d2l), 1);
            __ bind(L);
        }
            break;
        case Bytecodes::_d2f:
            __ cvtsd2ss(xmm0, xmm0);
            break;
        default:
            ShouldNotReachHere();
    }
}
void NormalCompileTask::lcmp(){
    Label done;
    __ pop_l(rdx);
    __ cmpq(rdx, rax);
    __ movl(rax, -1);
    __ jccb(Assembler::less, done);
    __ setb(Assembler::notEqual, rax);
    __ movzbl(rax, rax);
    __ bind(done);
}
void NormalCompileTask::float_cmp(int unordered_result) {
    float_cmp(true, unordered_result);
}
void NormalCompileTask::float_cmp(bool is_float, int unordered_result) {
    Label done;
    if (is_float) {
        // XXX get rid of pop here, use ... reg, mem32
        __ pop_f(xmm1);
        __ ucomiss(xmm1, xmm0);
    } else {
        // XXX get rid of pop here, use ... reg, mem64
        __ pop_d(xmm1);
        __ ucomisd(xmm1, xmm0);
    }
    if (unordered_result < 0) {
        __ movl(rax, -1);
        __ jccb(Assembler::parity, done);
        __ jccb(Assembler::below, done);
        __ setb(Assembler::notEqual, rdx);
        __ movzbl(rax, rdx);
    } else {
        __ movl(rax, 1);
        __ jccb(Assembler::parity, done);
        __ jccb(Assembler::above, done);
        __ movl(rax, 0);
        __ jccb(Assembler::equal, done);
        __ decrementl(rax);
    }
    __ bind(done);
}
void NormalCompileTask::double_cmp(int unordered_result) {
    float_cmp(false, unordered_result);
}
void NormalCompileTask::if_0cmp(NormalCompileTask::Condition cc) {
    __ testl(rax, rax);
    branch(false, false, cc);
    __ profile_not_taken_branch(rax);
}
void NormalCompileTask::if_icmp(NormalCompileTask::Condition cc) {
    __ pop_i(rdx);
    __ cmpl(rdx, rax);
    branch(false, false, cc);
}
void NormalCompileTask::if_acmp(NormalCompileTask::Condition cc) {
    // assume branch is more often taken than not (loops use backward branches)
    __ pop_ptr(rdx);
    __ cmpptr(rdx, rax);
    branch(false, false, cc);
    __ profile_not_taken_branch(rax);
}
void NormalCompileTask::if_nullcmp(NormalCompileTask::Condition cc) {
    // assume branch is more often taken than not (loops use backward branches)
    __ testptr(rax, rax);
    branch(false, false, cc);
    __ profile_not_taken_branch(rax);
}
void NormalCompileTask::_goto() {
    branch(false, false, none);
    tos = udtos;
}
void NormalCompileTask::jsr(){
    Unimplemented();
}
void NormalCompileTask::ret(){
    Unimplemented();
}
void NormalCompileTask::iinc(){
    signed char* value_pointer = (signed char*)bs->bcp() + 2;
    __ movl(rdx, *value_pointer); // get constant
    __ addl(iaddress(bs->get_index_u1()), rdx);
}
void NormalCompileTask::wide_iinc() {
    Unimplemented();
}
void NormalCompileTask::branch(bool is_jsr, bool is_wide, Condition c){
    int target = 0;
    if (is_wide)
        target = bs->get_far_dest();
    else
        target = bs->get_dest();

    if (is_jsr)
        Unimplemented();

    jump_target(target, c);
    will_run = false;
}
void NormalCompileTask::lmul() {
    __ pop_l(rdx);
    __ imulq(rax, rdx);
}
void NormalCompileTask::ldiv(){
    __ mov(rcx, rax);
    __ pop_l(rax);
    // generate explicit div0 check
    __ testq(rcx, rcx);
    __ jump_cc(Assembler::zero,
               ExternalAddress(Interpreter::_throw_ArithmeticException_entry));
    // Note: could xor rax and rcx and compare with (-1 ^ min_int). If
    //       they are not equal, one could do a normal division (no correction
    //       needed), which may speed up this implementation for the common case.
    //       (see also JVM spec., p.243 & p.271)
    __ corrected_idivq(rcx); // kills rbx
}
void NormalCompileTask::idiv(){
    __ movl(rcx, rax);
    __ pop_i(rax);
    // Note: could xor eax and ecx and compare with (-1 ^ min_int). If
    //       they are not equal, one could do a normal division (no correction
    //       needed), which may speed up this implementation for the common case.
    //       (see also JVM spec., p.243 & p.271)
    __ corrected_idivl(rcx);
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

    #ifdef DB_FRAME

      __ push(r11);
      __ mov(c_rarg0, r15_thread);
      __ mov(c_rarg1, r13);
      __ movptr(c_rarg2, (intptr_t)method);

      __ call_VME(CAST_FROM_FN_PTR(address, print_enclave_frame), false, true);

      __ pop(r11);
    #endif

    if (break_method != NULL &&
        strcmp(method->name()->as_C_string(), break_method) == 0 &&
        strcmp(method->klass_name()->as_C_string(), break_klass) == 0) {
        __ os_breakpoint();
    }

    // YYY
//   __ incrementl(rdx);
//   __ andl(rdx, -2);

    // see if we've got enough room on the stack for locals plus overhead.
//    generate_stack_overflow_check();

    // get return address
    __ pop(rax);

    // is ecall
    if (is_sgx_interface(method) || strncmp(method->name()->as_C_string(), "sgx_hook", 8) == 0) {
        for (int i = 0;i < size_parameters;i++)
        {
            __ movptr(r11, Address(r14, size_parameters - i, Address::times_8));
            __ push(r11);
        }
    }

    // compute beginning of parameters (r14)
    __ lea(r14, Address(rsp, size_parameters, Address::times_8, -wordSize));

    // rdx - # of additional locals
    // allocate space for locals
    // explicitly initialize locals
    for (int i = 0;i < addtional_locals;i++)
    {
        __ push((int) NULL_WORD); // initialize local variables
    }

    // initialize fixed part of activation frame
    generate_fixed_frame(false);

    // make sure method is not native & not abstract

    // Since at this point in the method invocation the exception
    // handler would try to exit the monitor of synchronized methods
    // which hasn't been entered yet, we set the thread local variable
    // _do_not_unlock_if_synchronized to true. The remove_activation
    // will check this flag.

//    const Address do_not_unlock_if_synchronized(r15_thread,
//                                                in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
//    __ movbool(do_not_unlock_if_synchronized, true);

    // check for synchronized interpreted methods
//    bang_stack_shadow_pages(false);

    // reset the _do_not_unlock_if_synchronized flag
//    __ movbool(do_not_unlock_if_synchronized, false);

}

void NormalCompileTask::return_entry(TosState state, int parameter_size) {

    Register parameter = rcx;

    // Restore stack bottom in case i2c adjusted stack
    __ movptr(rsp, Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize));
    // and NULL it as marker that esp is now tos until next java call
    __ movptr(Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize), (int32_t)NULL_WORD);

    __ restore_bcp();
    __ restore_locals();

//    __ movl(parameter, parameter_size);
    __ lea(rsp, Address(rsp, parameter_size, Interpreter::stackElementScale()));

    // the parameters plus the ret address
    __ current_entry->clear_bit_prev(parameter_size + 1);
}

void NormalCompileTask::_return(TosState state) {
  #ifdef DB_FRAME
    __ pusha();
    __ movptr(c_rarg1, (intptr_t)method);
    __ call_VME(CAST_FROM_FN_PTR(address, exit_enclave_frame), false, true);
    __ popa();
  #endif

    if (ret_tos == vtos)
        __ pop(state);

    // Narrow result if state is itos but result type is smaller.
    // Need to narrow in the return bytecode rather than in generate_return_entry
    // since compiled code callers expect the result to already be narrowed.
    narrow(rax, state);
    remove_activation(state, r13, true, true, true);

    __ ret(0);
    tos = state;
}

void NormalCompileTask::patch_jmp(address inst_addr, address jmp_addr) {
    if (inst_addr[0] == (unsigned char) 0x0F) {
        // condition jmp
        address code_pos = inst_addr + 1;
        intptr_t disp = (intptr_t)jmp_addr - ((intptr_t)code_pos + 1 + 4);

        *((int32_t *)(code_pos+1)) = (int32_t) disp;
        ICache::invalidate_range(inst_addr, NativeGeneralJump::instruction_size + 1);
    } else {
        NativeGeneralJump::insert_unconditional(inst_addr, jmp_addr);
    }
}

void NormalCompileTask::index_check(Register array, Register index) {
    // destroys rbx
    // check array
    __ null_check(array, arrayOopDesc::length_offset_in_bytes());
    // sign extend index for use by indexed load
    __ movl2ptr(index, index);
    // check index
    __ cmpl(index, Address(array, arrayOopDesc::length_offset_in_bytes()));
    if (index != rbx) {
        // ??? convention: move aberrant index into ebx for exception message
        assert(rbx != array, "different registers");
        __ movl(rbx, index);
    }
    __ jump_cc(Assembler::aboveEqual,
               ExternalAddress(Interpreter::_throw_ArrayIndexOutOfBoundsException_entry));
}
//void NormalCompileTask::locals_index(Register reg, int offset) { Unimplemented(); }

void NormalCompileTask::_new() {
    int idx = bs->get_index_u2();
    constantTag tag =  method->constants()->tag_at(idx);
    if (!tag.is_klass()) {
        if (will_run) {
            Bytecode_new bytecode_new(method, method->bcp_from(bs->bci()));
            Klass* klass = method->constants()->klass_at(bytecode_new.index(), JavaThread::current());
            klass->initialize(JavaThread::current());
            __ movptr(rdx, (intptr_t) klass);
        } else {
            // do the patch
            PatchingStub *patchingStub = new PatchingStub(_masm, PatchingStub::load_klass_id, bs->bci());
            __ movptr(rdx, NULL_WORD);
            patchingStub->install();
            append_stub(patchingStub);
        }
    } else {
        Klass* klass = method->constants()->klass_at_if_loaded(constantPoolHandle(method->constants()), idx);
        __ movptr(rdx, (intptr_t) klass);
    }
    gc_point();
    __ call(RuntimeAddress(Runtime0::entry_for(Runtime0::new_instance_id)));
}


void NormalCompileTask::newarray() {
    Register length = rbx;
    Register klass  = rdx;
    gc_point();
    __ movl(length, rax);
    __ movptr(rax, (intptr_t)Universe::typeArrayKlassObj());
    __ movptr(klass, Address(rax, (int32_t)bs->get_index_u1(), Address::times_ptr));

    gc_point();
    __ call(RuntimeAddress(Runtime0::entry_for(Runtime0::new_type_array_id)));
}

void NormalCompileTask::anewarray() {
    gc_point();
    int idx = bs->get_index_u2();
    __ movl(rbx, rax);
    constantTag tag =  method->constants()->tag_at(idx);
    if (!tag.is_klass()) {
        if (will_run) {
            Bytecode_anewarray anew(method, method->bcp_from(bs->bci()));
            Klass* ek = method->constants()->klass_at(anew.index(), JavaThread::current());
            Klass* klass = ek->array_klass(JavaThread::current());
            __ movptr(rdx, (intptr_t)klass);
        } else {
            // do the patch
            PatchingStub *patchingStub = new PatchingStub(_masm, PatchingStub::load_klass_id, bs->bci());
            __ movptr(rdx, NULL_WORD);
            patchingStub->install();
            append_stub(patchingStub);
        }
    } else {
        Klass* klass = method->constants()->klass_at_if_loaded(constantPoolHandle(method->constants()), idx);
        klass = klass->array_klass(JavaThread::current());
        __ movptr(rdx, (intptr_t) klass);
    }

    gc_point();
    __ call(RuntimeAddress(Runtime0::entry_for(Runtime0::new_object_array_id)));
}

void NormalCompileTask::arraylength() {
    __ null_check(rax, arrayOopDesc::length_offset_in_bytes());
    __ movl(rax, Address(rax, arrayOopDesc::length_offset_in_bytes()));
}

void NormalCompileTask::multianewarray() {
    gc_point();
    int nfold = bs->bcp()[3];
    __ lea(rcx, Address(rsp, nfold, Address::times_8, -wordSize));

    __ movptr(c_rarg1, (intptr_t)method->constants());
    __ movl(c_rarg2, bs->get_index_u2());
    __ mov(c_rarg3, rcx);
    __ movl(c_rarg4, nfold);

    __ call_VME(CAST_FROM_FN_PTR(address, EnclaveMemory::static_cpoll_multi_array));
    __ lea(rsp, Address(rsp, nfold, Address::times_8));
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
        __ pop(rax);

    const Register obj = rcx;

    int off = 0xa1;
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
        case btos: __ movb(field, rax); break;
        case ztos: __ movb(field, rax); break;
        case itos: __ movl(field, rax); break;
        case ctos: __ movw(field, rax); break;
        case stos: __ movw(field, rax); break;
        case ltos: __ movq(field, rax); break;
        case ftos: __ movflt(field, xmm0); break;
        case dtos: __ movdbl(field, xmm0); break;
        case atos: __ store_heap_oop(field, rax); break;
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
    const Register obj = rcx;
    int off = 0xa1;
    TosState tosState = ilgl;
    if (!is_static) {
        assert_different_registers(obj, rax);
        if (tos == vtos) {
            __ pop(atos);
        }
        __ mov(obj, rax);
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
        case btos: tos = btos; __ load_signed_byte(rax, field); break;
        case ztos: tos = ztos; __ load_signed_byte(rax, field); break;
        case itos: tos = itos; __ movl(rax, field); break;
        case ctos: tos = ctos; __ load_unsigned_short(rax, field); break;
        case stos: tos = stos; __ load_signed_short(rax, field); break;
        case ltos: tos = ltos; __ movq(rax, field); break;
        case ftos: tos = ftos; __ movflt(xmm0, field); break;
        case dtos: tos = dtos; __ movdbl(xmm0, field); break;
        case atos: tos = atos; __ load_heap_oop(rax, field); break;
        default:
            ShouldNotReachHere();
    }
    if (patching) {
        patching->install();
        append_stub(patching);
    }
}

PatchingStub* NormalCompileTask::resolve_cache_and_index(int byte_no, Register c_obj, int &off, TosState &tosState, bool is_static) {
    // for loop of code
    Klass* klass_holder = method->method_holder();
    if (!((InstanceKlass*)klass_holder)->is_initialized()) {
        klass_holder->initialize(JavaThread::current());
    }
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
            __ movptr(c_obj, (intptr_t)field_holder_klass->java_mirror());
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
    // how to do a invokevirtual call?
    // 1. resolve the function first
    // 2. compile the function, if not
    // 3. call the function
    // 4. put the function, jmp to / put the parameters
    // 5. the return handling of function

    // * need to change the calling convention, if necessary
    invoke(byte_no, rbx, rbx, rcx, rdx);
}

void NormalCompileTask::invokespecial(int byte_no) {
    invoke(byte_no, rbx, noreg, rcx, noreg);
}

void NormalCompileTask::invokeinterface(int byte_no) {
    invoke(byte_no, rbx, rbx, rcx, rdx);
}

void NormalCompileTask::invokestatic(int byte_no) {
    invoke(byte_no, rbx, noreg, noreg, noreg);
}

void NormalCompileTask::invokedynamic(int byte_no) {

}

void NormalCompileTask::invoke(int byte_no, Register m, Register index, Register recv, Register flags) {
    // get the address of the function call, if not resolve, then return a patching

    gc_point();

     if (tos != vtos) {
         __ push(tos);
     }

    int parameter_size = 0;
    const bool load_receiver    =   (recv   !=  noreg);
    // setup registers & access constant pool cache
    if (recv  == noreg)  recv  = rcx;
    if (flags == noreg)  flags = rdx;

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
        __ movptr(recv, Address(rsp, (-1 + parameter_size) * Interpreter::stackElementSize));
    }

    // find the Method*
    // if the Method is not resolved, then resolve and compile it
    // else if the method is not compiled, then compile it later
    // else call the method directly
    if (method_entry == NULL || !method_entry->is_resolved(bs->code())) {
        // method is not loaded, do the patch and then compile it
        PatchingStub *stub = new PatchingStub(_masm, PatchingStub::load_method_id, bs->bci());
        Label final;
        if (bs->code() == Bytecodes::_invokeinterface) {
            __ movptr(rax, NULL_WORD);
            __ movptr(index, 0xffff);
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
            __ movptr(m, NULL_WORD);
            __ jmp(final);
        } else {
            compiled_entry = (address)-1;
            __ movptr(m, NULL_WORD);
        }

        stub->install();
        append_stub(stub);

        if (bs->code() == Bytecodes::_invokeinterface) {
            __ load_klass(rdx, recv);
            __ lookup_interface_method(// inputs: rec. class, interface, itable index
                    rdx, rax, index,
                    // outputs: method, scan temp. reg
                    m, r13,
                    no_such_interface);
        } else if (bs->code() == Bytecodes::_invokevirtual) {
          __ load_klass(rax, recv);
          __ lookup_virtual_method(rax, index, m);
          __ bind(final);
        }

    } else {

        parameter_size = MetadataAccessor::method_parameter_size(method, bs->bci(), called_tos);

        if (load_receiver) {
            parameter_size += 1;
            __ movptr(recv, Address(rsp, (-1 + parameter_size) * Interpreter::stackElementSize));
        }

        if (bs->code() == Bytecodes::_invokeinterface) {
            interface_klass = method_entry->f1_as_klass();
            vtable_index = method_entry->f2_as_index();

            if (method_entry->is_forced_virtual()) {
                    __ load_klass(rax, recv);
                    __ lookup_virtual_method(rax, vtable_index, m);
                    force_compile = false;
            } else {
                __ load_klass(rdx, recv);
                __ movptr(rax, (intptr_t)interface_klass);

                __ lookup_interface_method(// inputs: rec. class, interface, itable index
                        rdx, rax, vtable_index,
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
                    __ load_klass(rax, recv);
                    __ lookup_virtual_method(rax, vtable_index, m);
                }
            }

            if (callee) {
                if (will_run) {
                    EnclaveRuntime::compile_method(callee);
                }
                // load Method*, if invokestatic or vfinal
                __ movptr(m, (intptr_t)callee);
                if (JCompiler::is_compile(callee)) {
                    compiled_entry = callee->from_compiled_entry();
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
    // if (bs->code() == Bytecodes::_invokeinterface) {
    //     __ bind(no_such_interface);
    //     __ movptr(rax, (intptr_t)-1);
    //     __ jmp(rax);
    // }

    return_entry(tosState, parameter_size);
    tos = called_tos;
    if (patch_compile != NULL) {
        append_stub(patch_compile);
    }
}

void NormalCompileTask::checkcast() {
    Label done, is_null, ok_is_subtype, quicked, resolved;

    __ testptr(rax, rax); // object is in rax
    __ jcc(Assembler::zero, is_null);
    // rax: obj
    int idx = bs->get_index_u2();
    constantTag tag =  method->constants()->tag_at(idx);
    if (tag.is_klass()) {
       __ mov(rdx, rax);
       __ movptr(rax, (intptr_t)method->constants()->klass_at_if_loaded(constantPoolHandle(method->constants()), idx));
    } else {
        __ mov(rdx, rax);
        if (will_run) {
            Bytecode_checkcast cc(method, method->bcp_from(bs->bci()));
            Klass* klass = method->constants()->klass_at(cc.index(), JavaThread::current());
            __ movptr(rax, (intptr_t)klass);
        } else {
            PatchingStub *stub = new PatchingStub(_masm, PatchingStub::load_klass_id, bs->bci());
            __ movptr(rax, NULL_WORD);
            stub->install();
            append_stub(stub);
        }
    }

    // rax: klass
    // rdx: obj

    __ load_klass(rbx, rdx);

    // Generate subtype check.  Blows rcx, rdi.  Object in rdx.
    // Superklass in rax.  Subklass in rbx.
    __ gen_subtype_check(rbx, ok_is_subtype);

    // Come here on failure
    __ push_ptr(rdx);
    // object is at TOS
    __ jump(ExternalAddress(Interpreter::_throw_ClassCastException_entry));

    // Come here on success
    __ bind(ok_is_subtype);
    __ mov(rax, rdx); // Restore object in rdx

    // Collect counts on whether this check-cast sees NULLs a lot or not.
    if (ProfileInterpreter) {
        __ jmp(done);
        __ bind(is_null);
        __ profile_null_seen(rcx);
    } else {
        __ bind(is_null);   // same as 'done'
    }
    __ bind(done);
}

void NormalCompileTask::instanceof() {

    Label done, is_null, ok_is_subtype, quicked, resolved;
    __ testptr(rax, rax);
    __ jcc(Assembler::zero, is_null);

    // rax: obj
    int idx = bs->get_index_u2();
    constantTag tag =  method->constants()->tag_at(idx);
    if (tag.is_klass()) {
        __ mov(rdx, rax);
        __ movptr(rax, (intptr_t)method->constants()->klass_at_if_loaded(constantPoolHandle(method->constants()), idx));
    } else {
        __ mov(rdx, rax);
        if (will_run) {
            Bytecode_instanceof io(method, method->bcp_from(bs->bci()));
            Klass* k = method->constants()->klass_at(io.index(), JavaThread::current());
            __ movptr(rax, (intptr_t)k);
        } else {
            PatchingStub *stub = new PatchingStub(_masm, PatchingStub::load_klass_id, bs->bci());
            __ movptr(rax, NULL_WORD);
            stub->install();
            append_stub(stub);
        }
    }
    __ load_klass(rdx, rdx);
    // rdx: klass of obj
    // rax: klass of instanceof klass


    // Generate subtype check.  Blows rcx, rdi
    // Superklass in rax.  Subklass in rdx.
    __ gen_subtype_check(rdx, ok_is_subtype);

    // Come here on failure
    __ xorl(rax, rax);
    __ jmpb(done);
    // Come here on success
    __ bind(ok_is_subtype);
    __ movl(rax, 1);

    // Collect counts on whether this test sees NULLs a lot or not.
    if (ProfileInterpreter) {
        __ jmp(done);
        __ bind(is_null);
        __ profile_null_seen(rcx);
    } else {
        __ bind(is_null);   // same as 'done'
    }
    __ bind(done);
}

void NormalCompileTask::gc_point() {
    __ movptr(Address(rbp, frame::interpreter_frame_bcx_offset * wordSize), bs->bci());
    oopSet->put_entry(bs->bci(), __ current_entry->clone());
}

void NormalCompileTask::lookupswitch() {
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
    __ cmpl(rax, target_key);
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
    uint32_t* offsets = (int32_t*)(offset_start);

    int low = Bytes::get_Java_u4((address)(offsets + 1));
    int high = Bytes::get_Java_u4((address)(offsets + 2));
    int default_bci_offset = Bytes::get_Java_u4((address)(offsets + 0));

    // compare low
    __ cmpl(rax, low);
    jump_target(bs->bci() + default_bci_offset, less);
    // compare high
    __ cmpl(rax, high);
    jump_target(bs->bci() + default_bci_offset, greater);
    __ subl(rax, low);

    // store the dispatch table
    address mov_dispatch_table = __ pc();
    __ movptr(rdx, NULL);
    __ movptr(rax, Address(rdx, rax, Address::times_ptr));
    __ jmp(rax);
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
        if (cc == none) {
            __ emit_int8((unsigned char) 0xE9);
            __ emit_int32(itr->second - next_addr);
        } else {
            next_addr += 1;
            __ emit_int8((unsigned char) 0x0F);
            __ emit_int8((unsigned char) (0x80 | cc));
            __ emit_int32(itr->second - next_addr);
        }
    } else {
        patch_address.push_back(std::pair<int, address>(target, __ pc()));
        if (cc == none) {
            __ emit_int8((unsigned char) 0xE9);
            __ emit_int32(0);
        } else {
            __ emit_int8((unsigned char) 0x0F);
            __ emit_int8((unsigned char) (0x80 | cc));
            __ emit_int32(0);
        }
    }

}

void NormalCompileTask::remove_activation(TosState state, Register ret_addr, bool throw_monitor_exception,
                                          bool install_monitor_exception, bool notify_jvmdi) {
    // Note: Registers rdx xmm0 may be in use for the
    // result check if synchronized method
//    Label unlocked, unlock, no_unlock;
//
//    // get the value of _do_not_unlock_if_synchronized into rdx
//    const Address do_not_unlock_if_synchronized(r15_thread,
//                                                in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
//    __ movbool(rdx, do_not_unlock_if_synchronized);
//    __ movbool(do_not_unlock_if_synchronized, false); // reset the flag
//
//    // get method access flags
//    if (method->is_synchronized()) {
//        // Don't unlock anything if the _do_not_unlock_if_synchronized flag
//        // is set.
//        __ testbool(rdx);
//        __ jcc(Assembler::notZero, no_unlock);
//
//        // unlock monitor
//        __ push(state); // save result
//
//        // BasicObjectLock will be first in list, since this is a
//        // synchronized method. However, need to check that the object has
//        // not been unlocked by an explicit monitorexit bytecode.
//        const Address monitor(rbp, frame::interpreter_frame_initial_sp_offset *
//                                   wordSize - (int) sizeof(BasicObjectLock));
//        // We use c_rarg1 so that if we go slow path it will be the correct
//        // register for unlock_object to pass to VM directly
//        __ lea(c_rarg1, monitor); // address of first monitor
//
//        __ movptr(rax, Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()));
//        __ testptr(rax, rax);
//        __ jcc(Assembler::notZero, unlock);
//
//        __ pop(state);
//        if (throw_monitor_exception) {
//            // Entry already unlocked, need to throw exception
//            __ call_VM(noreg, CAST_FROM_FN_PTR(address,
//                                            InterpreterRuntime::throw_illegal_monitor_state_exception));
//            __ should_not_reach_here();
//        } else {
//            // Monitor already unlocked during a stack unroll. If requested,
//            // install an illegal_monitor_state_exception.  Continue with
//            // stack unrolling.
//            if (install_monitor_exception) {
//                __ call_VM(noreg, CAST_FROM_FN_PTR(address,
//                                                InterpreterRuntime::new_illegal_monitor_state_exception));
//            }
//            __ jmp(unlocked);
//        }
//
//        __ bind(unlock);
//        __ unlock_object(c_rarg1);
//        __ pop(state);
//    }
//
//    // Check that for block-structured locking (i.e., that all locked
//    // objects has been unlocked)
//    __ bind(unlocked);
//
//    // rax: Might contain return value
//
//    // Check that all monitors are unlocked
//    {
//        Label loop, exception, entry, restart;
//        const int entry_size = frame::interpreter_frame_monitor_size() * wordSize;
//        const Address monitor_block_top(
//                rbp, frame::interpreter_frame_monitor_block_top_offset * wordSize);
//        const Address monitor_block_bot(
//                rbp, frame::interpreter_frame_initial_sp_offset * wordSize);
//
//        __ bind(restart);
//        // We use c_rarg1 so that if we go slow path it will be the correct
//        // register for unlock_object to pass to VM directly
//        __ movptr(c_rarg1, monitor_block_top); // points to current entry, starting
//        // with top-most entry
//        __ lea(rbx, monitor_block_bot);  // points to word before bottom of
//        // monitor block
//        __ jmp(entry);
//
//        // Entry already locked, need to throw exception
//        __ bind(exception);
//
//        if (throw_monitor_exception) {
//            // Throw exception
////            __ call_VM(noreg,
////                                    CAST_FROM_FN_PTR(address, InterpreterRuntime::
////                                            throw_illegal_monitor_state_exception));
////            __ should_not_reach_here();
//        } else {
//            // Stack unrolling. Unlock object and install illegal_monitor_exception.
//            // Unlock does not block, so don't have to worry about the frame.
//            // We don't have to preserve c_rarg1 since we are going to throw an exception.
//
//            __ push(state);
//            __ unlock_object(c_rarg1);
//            __ pop(state);
//
//            if (install_monitor_exception) {
//                __ call_VM(noreg, CAST_FROM_FN_PTR(address,
//                                                InterpreterRuntime::
//                                                        new_illegal_monitor_state_exception));
//            }
//
//            __ jmp(restart);
//        }
//
//        __ bind(loop);
//        // check if current entry is used
//        __ cmpptr(Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()), (int32_t) NULL);
//        __ jcc(Assembler::notEqual, exception);
//
//        __ addptr(c_rarg1, entry_size); // otherwise advance to next entry
//        __ bind(entry);
//        __ cmpptr(c_rarg1, rbx); // check if bottom reached
//        __ jcc(Assembler::notEqual, loop); // if not at bottom then check this entry
//    }
//
//    __ bind(no_unlock);


    // remove activation
    // get sender sp
    __ movptr(rbx,
           Address(rbp, frame::interpreter_frame_sender_sp_offset * wordSize));
    __ leave();                           // remove frame anchor
    __ pop(ret_addr);                     // get return address
    __ mov(rsp, rbx);                     // set sp to sender sp
    __ push(ret_addr);
}

void NormalCompileTask::narrow(Register result, TosState tos) {
    switch (tos) {
        case ztos:  __ andl(result, (int32_t)0x1);    break;
        case stos:
            LP64_ONLY(__ movswl(result, result);)
            NOT_LP64(shll(result, 16);)      // truncate upper 16 bits
            NOT_LP64(sarl(result, 16);)      // and sign-extend short
            break;
        case btos:
            LP64_ONLY(__ movsbl(result, result);)
            NOT_LP64(shll(result, 24);)      // truncate upper 24 bits
            NOT_LP64(sarl(result, 24);)      // and sign-extend byte
            break;
        case ctos:
            LP64_ONLY(__ movzwl(result, result);)
            NOT_LP64(__ andl(result, 0xFFFF);)  // truncate upper 16 bits
            break;
        default:                break;
    }
}

void NormalCompileTask::_jmp_return() {
    __ jmp(ret_now);
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

void NormalCompileTask::athrow() {
    // crash the system when a exception is throw
    __ pop(atos);
    __ movptr(rax, (intptr_t)NULL);
    __ jmp(rax);
    tos = udtos;
}

void NormalCompileTask::monitorenter() {
    // rax cache the object
    Label retry;

    __ lea(rcx, Address(rax, oopDesc::mark_offset_in_bytes()));

    __ bind(retry);
    __ mov(rdx, rcx);
    
    __ movptr(rax, Address(rdx, 0));
    __ movptr(rdx, rax);
    __ andptr(rdx, markOopDesc::lock_mask_in_place);
    __ cmpptr(rdx, markOopDesc::unlocked_value);
    __ jcc(Assembler::notEqual, retry);

    __ movptr(rdx, rax);

    // clear lock
    __ andptr(rdx, ~markOopDesc::lock_mask_in_place);

    if (os::is_MP()) {
        __ lock();
    }
    __ cmpxchgptr(rax, Address(rcx, 0));
    __ jcc(Assembler::notEqual, retry);
}

void NormalCompileTask::monitorexit() {
    // rax cache the object
    __ movptr(rdx, Address(rax, oopDesc::mark_offset_in_bytes()));
    __ orptr(rdx, markOopDesc::unlocked_value);
    __ movptr(Address(rax, markOopDesc::mark_offset_in_bytes()), rdx);
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

// fast_compile compile method less than 6 instructions (typically setter and getter)
int NormalCompileTask::fast_compile() {
    int ins_count = 0;
    int depth = 0;

    // no extra locals
    if (method->max_locals() - method->size_of_parameters() > 0) {
        return false;
    }

    if (method->is_synchronized()) {
        return false;
    }

    if (is_sgx_interface(method) || strncmp(method->name()->as_C_string(), "sgx_hook", 8) == 0) {
        return false;
    }

    while (true) {
        Bytecodes::Code code = bs->next();
        if (code == Bytecodes::_illegal) {
            break;
        }
        ins_count ++;

        // no jmps, no klass, no function call, less than 6 insts
        if (is_jump_code(code) || is_invoke_code(code) || is_klass_code(code) || ins_count >= 6) {
            return false;
        }

        switch (code) {
            case Bytecodes::_nop:           break;
            case Bytecodes::_aconst_null:
            case Bytecodes::_iconst_0:
            case Bytecodes::_iconst_m1:
            case Bytecodes::_iconst_1:
            case Bytecodes::_iconst_2:
            case Bytecodes::_iconst_3:
            case Bytecodes::_iconst_4:
            case Bytecodes::_iconst_5:
            case Bytecodes::_lconst_0:
            case Bytecodes::_lconst_1:
            case Bytecodes::_fconst_0:
            case Bytecodes::_fconst_1:
            case Bytecodes::_fconst_2:
            case Bytecodes::_dconst_0:
            case Bytecodes::_dconst_1:
                depth += 1;
                break;
            case Bytecodes::_iload_0:
            case Bytecodes::_iload_1:
            case Bytecodes::_iload_2:
            case Bytecodes::_iload_3:
            case Bytecodes::_lload_0:
            case Bytecodes::_lload_1:
            case Bytecodes::_lload_2:
            case Bytecodes::_lload_3:
            case Bytecodes::_fload_0:
            case Bytecodes::_fload_1:
            case Bytecodes::_fload_2:
            case Bytecodes::_fload_3:
            case Bytecodes::_dload_0:
            case Bytecodes::_dload_1:
            case Bytecodes::_dload_2:
            case Bytecodes::_dload_3:
            case Bytecodes::_aload_0:
            case Bytecodes::_aload_1:
            case Bytecodes::_aload_2:
            case Bytecodes::_aload_3:
                depth += 1;
                break;
            case Bytecodes::_ireturn:
            case Bytecodes::_lreturn:
            case Bytecodes::_areturn:
            case Bytecodes::_return:
            case Bytecodes::_freturn:
            case Bytecodes::_dreturn:

                break;
            case Bytecodes::_getstatic: depth += 1; break;
            case Bytecodes::_putstatic: depth -= 1; break;
            case Bytecodes::_getfield:  depth += 0; break;
            case Bytecodes::_putfield:  depth -= 2; break;
            default:
                return false;
        }

        // tos stack less than 2
        if (depth > 2) {
            return false;
        }
    }

    BufferBlob* stub_blob = BufferBlob::create("FastTask:",  50);
    if (stub_blob == NULL) {
        ShouldNotReachHere();
    }
    CodeBuffer c(stub_blob);
    _masm = new C0_MacroAssembler(&c, new C0MapOopEntry(method));

    method_entry = __ pc();

    // get ret address
    __ pop(r11);

    __ lea(r14, Address(rsp, method->size_of_parameters(), Address::times_8, -wordSize));

    bs->set_next_bci(0);
    Bytecodes::Code code;
    int register_loaded[4];
    AbstractRegisterImpl* tos_reg[2];
    tos_reg[0] = rscratch1;
    tos_reg[1] = rscratch2;
    int tos_count = 0;
    int tos_used = false;
    bool is_static = false;
    bool is_put = false;
    while (true) {
        code = bs->next();
        if (code == Bytecodes::_illegal) {
            break;
        }
        // rax -> tos
        // rbx -> 0
        // rcx -> 1
        // rdx -> 2
        // rdi -> 3

        if (code >= Bytecodes::_aconst_null && code <= Bytecodes::_dconst_1) {
            if (tos_used) {
                printf(D_ERROR("Fast Compile")"rax is used\n\n\n");
                return false;
            }
            tos_used = true;
        }

        switch (code) {
            case Bytecodes::_nop:                                   break;
            case Bytecodes::_aconst_null:   aconst_null();  tos_reg[tos_count++] = rax;     break;
            case Bytecodes::_iconst_0:      iconst(0);      tos_reg[tos_count++] = rax;     break;
            case Bytecodes::_iconst_m1:     iconst(-1);     tos_reg[tos_count++] = rax;     break;
            case Bytecodes::_iconst_1:      iconst(1);      tos_reg[tos_count++] = rax;     break;
            case Bytecodes::_iconst_2:      iconst(2);      tos_reg[tos_count++] = rax;     break;
            case Bytecodes::_iconst_3:      iconst(3);      tos_reg[tos_count++] = rax;     break;
            case Bytecodes::_iconst_4:      iconst(4);      tos_reg[tos_count++] = rax;     break;
            case Bytecodes::_iconst_5:      iconst(5);      tos_reg[tos_count++] = rax;     break;
            case Bytecodes::_lconst_0:      lconst(0);      tos_reg[tos_count++] = rax;     break;
            case Bytecodes::_lconst_1:      lconst(1);      tos_reg[tos_count++] = rax;     break;
            case Bytecodes::_fconst_0:      fconst(0);      tos_reg[tos_count++] = xmm0;    break;
            case Bytecodes::_fconst_1:      fconst(1);      tos_reg[tos_count++] = xmm0;    break;
            case Bytecodes::_fconst_2:      fconst(2);      tos_reg[tos_count++] = xmm0;    break;
            case Bytecodes::_dconst_0:      dconst(0);      tos_reg[tos_count++] = xmm0;    break;
            case Bytecodes::_dconst_1:      dconst(1);      tos_reg[tos_count++] = xmm0;    break;

            case Bytecodes::_iload_0:       __ movl(rbx, iaddress(0));      tos_reg[tos_count++] = rbx;     break;
            case Bytecodes::_iload_1:       __ movl(rcx, iaddress(1));      tos_reg[tos_count++] = rcx;     break;
            case Bytecodes::_iload_2:       __ movl(rdx, iaddress(2));      tos_reg[tos_count++] = rdx;     break;
            case Bytecodes::_iload_3:       __ movl(rdi, iaddress(3));      tos_reg[tos_count++] = rdi;     break;
            case Bytecodes::_lload_0:       __ movq(rbx, laddress(0));      tos_reg[tos_count++] = rbx;     break;
            case Bytecodes::_lload_1:       __ movq(rcx, laddress(1));      tos_reg[tos_count++] = rcx;     break;
            case Bytecodes::_lload_2:       __ movq(rdx, laddress(2));      tos_reg[tos_count++] = rdx;     break;
            case Bytecodes::_lload_3:       __ movq(rdi, laddress(3));      tos_reg[tos_count++] = rdi;     break;
            case Bytecodes::_fload_0:       __ movflt(xmm1, faddress(0));   tos_reg[tos_count++] = xmm1;    break;
            case Bytecodes::_fload_1:       __ movflt(xmm2, faddress(1));   tos_reg[tos_count++] = xmm2;    break;
            case Bytecodes::_fload_2:       __ movflt(xmm3, faddress(2));   tos_reg[tos_count++] = xmm3;    break;
            case Bytecodes::_fload_3:       __ movflt(xmm4, faddress(3));   tos_reg[tos_count++] = xmm4;    break;
            case Bytecodes::_dload_0:       __ movdbl(xmm1, daddress(0));   tos_reg[tos_count++] = xmm1;    break;
            case Bytecodes::_dload_1:       __ movdbl(xmm2, daddress(1));   tos_reg[tos_count++] = xmm2;    break;
            case Bytecodes::_dload_2:       __ movdbl(xmm3, daddress(2));   tos_reg[tos_count++] = xmm3;    break;
            case Bytecodes::_dload_3:       __ movdbl(xmm4, daddress(3));   tos_reg[tos_count++] = xmm4;    break;
            case Bytecodes::_aload_0:       __ movptr(rbx, aaddress(0));    tos_reg[tos_count++] = rbx;     break;
            case Bytecodes::_aload_1:       __ movptr(rcx, aaddress(1));    tos_reg[tos_count++] = rcx;     break;
            case Bytecodes::_aload_2:       __ movptr(rdx, aaddress(2));    tos_reg[tos_count++] = rdx;     break;
            case Bytecodes::_aload_3:       __ movptr(rdi, aaddress(3));    tos_reg[tos_count++] = rdi;     break;
            case Bytecodes::_lreturn:
                if (tos_reg[tos_count - 1] != rax) {
                    __ movq(rax, (Register)tos_reg[tos_count - 1]);
                }
                break;
            case Bytecodes::_ireturn:
                if (tos_reg[tos_count - 1] != rax) {
                    __ movl(rax, (Register)tos_reg[tos_count - 1]);
                }
                narrow(rax, MetadataAccessor::basicType2tosState(method->result_type()));
                break;
            case Bytecodes::_areturn:
                if (tos_reg[tos_count - 1] != rax) {
                    __ mov(rax, (Register)tos_reg[tos_count - 1]);
                }
            case Bytecodes::_return:    break;
            case Bytecodes::_freturn:
                if (tos_reg[tos_count - 1] != xmm0) {
                    __ movflt(xmm0, (XMMRegister)tos_reg[tos_count - 1]);
                }
                break;
            case Bytecodes::_dreturn:
                if (tos_reg[tos_count - 1] != xmm0) {
                    __ movdbl(xmm0, (XMMRegister)tos_reg[tos_count - 1]);
                }
                break;
            case Bytecodes::_getstatic:     is_static = true;   is_put = false; break;
            case Bytecodes::_putstatic:     is_static = true;   is_put = true;  break;
            case Bytecodes::_getfield:      is_static = false;  is_put = false; break;
            case Bytecodes::_putfield:      is_static = false;  is_put = true;  break;
            default:
                break;
        }

        if (code >= Bytecodes::_getstatic && code <= Bytecodes::_putfield) {
            // load the address offset and the klass to be loaded,
            // it seems that putfield do not need to resolve it
            Register c_obj = rsi;
            int off = 0xff;
            ConstantPoolCacheEntry *field_entry = method->constants()->cache()->entry_at(getfield_index());
            TosState tosState = MetadataAccessor::basicType2tosState(
                    MetadataAccessor::get_field_type_by_index(method->constants(), getfield_index()));
            Klass* field_holder_klass;
            Register recv_r;
            AbstractRegisterImpl* val_r;
            if (is_static) {
                field_holder_klass = MetadataAccessor::get_field_holder_klass_if_loaded(method->constants(), getfield_index());
                if (field_holder_klass == NULL) {
                    Bytecode_field field_access(methodHandle(method), bs->bci());
                    fieldDescriptor result; // initialize class if needed
                    Bytecodes::Code code = field_access.code();
                    constantPoolHandle constants(JavaThread::current(), method->constants());
                    LinkResolver::resolve_field_access(result, constants, field_access.index(), Bytecodes::java_code(code), JavaThread::current());
                    field_holder_klass = result.field_holder();
                }
                __ movptr(c_obj, (intptr_t)field_holder_klass->java_mirror());
                recv_r = c_obj;
                val_r = tos_reg[0];
            } else {
                recv_r = (Register)tos_reg[0];
                val_r = tos_reg[1];
                tos_count -= 1;
            }

            if (field_entry == NULL || !field_entry->is_resolved(bs->code())) {
                Bytecode_field field_access(methodHandle(method), bs->bci());
                fieldDescriptor result; // initialize class if needed
                Bytecodes::Code code = field_access.code();
                constantPoolHandle constants(JavaThread::current(), method->constants());
                LinkResolver::resolve_field_access(result, constants, field_access.index(), Bytecodes::java_code(code), JavaThread::current());
                off = result.offset();
            } else {
                off = field_entry->f2_as_index();
            }
            Address field(recv_r, off);
            // depends on the field type
            // should not be tos
            if (is_put) {
                if (val_r == rax || val_r == xmm0) {
                    tos_used = false;
                }
                tos_count -= 1;
                switch(tosState) {
                    case btos: __ movb(field, (Register)val_r); break;
                    case ztos: __ movb(field, (Register)val_r); break;
                    case itos: __ movl(field, (Register)val_r); break;
                    case ctos: __ movw(field, (Register)val_r); break;
                    case stos: __ movw(field, (Register)val_r); break;
                    case ltos: __ movq(field, (Register)val_r); break;
                    case ftos: __ movflt(field, (XMMRegister)val_r); break;
                    case dtos: __ movdbl(field, (XMMRegister)val_r); break;
                    case atos: __ store_heap_oop(field, (Register)val_r); break;
                    default:
                        ShouldNotReachHere();
                }
            } else {
                switch(tosState) {
                    case btos: tos = btos; __ load_signed_byte(rax, field);     tos_reg[tos_count++] = rax; break;
                    case ztos: tos = ztos; __ load_signed_byte(rax, field);     tos_reg[tos_count++] = rax; break;
                    case itos: tos = itos; __ movl(rax, field);                 tos_reg[tos_count++] = rax; break;
                    case ctos: tos = ctos; __ load_unsigned_short(rax, field);  tos_reg[tos_count++] = rax; break;
                    case stos: tos = stos; __ load_signed_short(rax, field);    tos_reg[tos_count++] = rax; break;
                    case ltos: tos = ltos; __ movq(rax, field);                 tos_reg[tos_count++] = rax; break;
                    case ftos: tos = ftos; __ movflt(xmm0, field);              tos_reg[tos_count++] = xmm0;break;
                    case dtos: tos = dtos; __ movdbl(xmm0, field);              tos_reg[tos_count++] = xmm0;break;
                    case atos: tos = atos; __ load_heap_oop(rax, field);        tos_reg[tos_count++] = rax; break;
                    default:
                        ShouldNotReachHere();
                }
            }

        }
    }

    // reset sender and ret
    __ mov(rsp, r13);
    __ push(r11);
    __ ret(0);

//    printf(D_INFO("Fast Compile")" %s\n", method->name()->as_C_string());
//
//    Disassembler disassembler(method_entry, __ offset());
//    disassembler.print_asm();
//
//    printf("\n");

    return true;
}

#undef __
