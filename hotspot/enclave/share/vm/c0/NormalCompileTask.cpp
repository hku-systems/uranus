//
// Created by jianyu on 11/11/18.
//

#include <precompiled.hpp>
#include <classfile/symbolTable.hpp>
#include "enclave/sc/EnclaveDebug.h"
#include "oops/markOop.hpp"
#include <interpreter/bytecode.hpp>
#include <code/codeBlob.hpp>
#include <runtime/sharedRuntime.hpp>
#include <nativeInst_aarch64.hpp>
#include <runtime/fieldType.hpp>
#include <enclave/sc/EnclaveNative.h>
#include "interpreter/interpreter.hpp"
#include "NormalCompileTask.h"
#include "bytecodeStream.hpp"
#include "c0_CodeStubs.hpp"
#include "Disassembler.hpp"
#include "MetadataAccessor.hpp"
#include "c0_Runtime.hpp"

#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "interpreter/templateTable.hpp"

#include "oops/method.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/oop.inline.hpp"

#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"

#include "interpreter/bytecodes.hpp"

#include <interpreter/interpreterRuntime.hpp>
#define __ _masm->
#define transition(from, to)

// 1. If we expect vtos, push rax into stacks
// 2. If we expect non-tos, pop to rax
// 3. If we have (vtos, vtos), the tos is decided in the generator

int NormalCompileTask::break_bci = 0;
char* NormalCompileTask::break_method = "growTable";
char* NormalCompileTask::break_klass = "org/apache/spark/util/collection/AppendOnlyMap";

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

void NormalCompileTask::checkcast_state(TosState tos, TosState intos) {
    TosState cast_tos;
    TosState cast_intos;
    switch (tos) {
        case itos:
        case btos:
        case ctos:
        case ztos:
        case stos:  cast_tos = itos;  break;
        case ftos:  cast_tos = ftos;  break;
        case dtos:  cast_tos = dtos;  break;
        case atos:  cast_tos = atos;  break;
        case ltos:  cast_tos = ltos;  break;
        case vtos:  cast_tos = vtos;  break;
        default:
            ShouldNotReachHere();
            cast_tos = ilgl;
    }
    switch (intos) {
        case itos:
        case btos:
        case ctos:
        case ztos:
        case stos:  cast_intos = itos;  break;
        case ftos:  cast_intos = ftos;  break;
        case dtos:  cast_intos = dtos;  break;
        case atos:  cast_intos = atos;  break;
        case ltos:  cast_intos = ltos;  break;
        case vtos:  cast_intos = vtos;  break;
        default:
            ShouldNotReachHere();
            cast_intos = ilgl;
    }
    if (cast_tos != cast_intos) {
        printf("in method %s:%s:%d %d: tos %d\n",method->klass_name()->as_C_string(), method->name()->as_C_string(), bs->bci(), cast_tos, cast_intos);
        ShouldNotReachHere();
    }
}

#define gen(func, intos, otos) {                    \
    if (tos >= number_of_states)                    \
        printf("error in bci %d\n", bs->bci());     \
    if (!(intos == otos && intos == vtos)) {        \
        if (intos == vtos && tos != vtos) {         \
            switch (tos) {                          \
                case atos: __ push_ptr();   break;  \
                case ftos: __ push_f();     break;  \
                case dtos: __ push_d();     break;  \
                case ltos: __ push_l();     break;  \
                case vtos:                  break;  \
                case itos:                          \
                case btos:                          \
                case ctos:                          \
                case ztos:                          \
                case stos: __ push_i();     break;  \
                    printf("error\n");              \
                    break;                          \
                default: printf("error\n"); break;  \
            }                                       \
            tos = vtos;                             \
        } else if (tos == vtos && intos != vtos) {  \
            switch (intos) {                        \
                case atos: __ pop_ptr();    break;  \
                case ftos: __ pop_f();      break;  \
                case dtos: __ pop_d();      break;  \
                case ltos: __ pop_l();      break;  \
                case vtos:                  break;  \
                case itos:                          \
                case btos:                          \
                case ctos:                          \
                case ztos:                          \
                case stos: __ pop_i();      break;  \
                default:                    break;  \
            }                                       \
            tos = intos;                            \
        } else {                                    \
            checkcast_state(tos, intos);            \
            tos = intos;                            \
        }                                           \
    }                                               \
    func;                                           \
    if (!(intos == otos && intos == vtos)) {        \
        tos = otos;                                 \
    }                                               \
    break;                                          \
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
        case NormalCompileTask::equal        : return Assembler::EQ;
        case NormalCompileTask::not_equal    : return Assembler::NE;
        case NormalCompileTask::less         : return Assembler::LT;
        case NormalCompileTask::less_equal   : return Assembler::LE;
        case NormalCompileTask::greater      : return Assembler::GT;
        case NormalCompileTask::greater_equal: return Assembler::GE;
    }
    ShouldNotReachHere();
    return Assembler::EQ;
}


// Miscelaneous helper routines
// Store an oop (or NULL) at the Address described by obj.
// If val == noreg this means store a NULL
static void do_oop_store(InterpreterMacroAssembler* _masm,
                         Address obj,
                         Register val,
                         BarrierSet::Name barrier,
                         bool precise) {
    assert(val == noreg || val == r0, "parameter is just for looks");
    switch (barrier) {
        case BarrierSet::Other:
            if (val == noreg) {
                __ store_heap_oop_null(obj);
            } else {
                __ store_heap_oop(obj, val);
            }
            break;
        default      :
            ShouldNotReachHere();

    }
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

int NormalCompileTask::compile(int size) {
    if (size == 0)
        size = 10240;
    BufferBlob* stub_blob = BufferBlob::create("EnclaveABI:", size + 20);
    if (stub_blob == NULL) {
        ShouldNotReachHere();
    }
    CodeBuffer c(stub_blob);
    _masm = new C0_MacroAssembler(&c, new C0MapOopEntry(method));

    jmp_target.clear();

    bs->set_next_bci(0);

    entry();

#ifdef ENCLAVE_BOUND_CHECK
#ifndef ENCLAVE_MPX
    char* start_heap = (char*)get_enclave_heap();
    char* end_heap = start_heap + get_enclave_heap_size();
    // last 2 registers to hold argument values
    // movptr change to str
    __ str(r6, (intptr_t)start_heap);
    __ str(r7, (intptr_t)end_heap);
#endif
#endif

    __ start_expression_stack();

    while (true) {
        Bytecodes::Code code = bs->next();
        if (code == Bytecodes::_illegal)
            break;

        if (break_method != NULL &&
            strcmp(method->name()->as_C_string(), break_method) == 0 &&
            strcmp(method->klass_name()->as_C_string(), break_klass) == 0 &&
            break_bci == bs->bci()) {
            //__ os_breakpoint();
            //Temporarily comment because it is for debug in macroAssembler
        }

        adjust_tos();

        bci_ptr_map.insert(std::pair<int, address>(bs->bci(), __ pc()));
        bci_tos.insert(std::pair<int, TosState>(bs->bci(), tos));

        switch (code) {
            case Bytecodes::_nop:			gen(nop(),		    vtos, vtos);
            case Bytecodes::_aconst_null:	gen(aconst_null(),  vtos, atos);
            case Bytecodes::_iconst_0:      gen(iconst(0),      vtos, itos);
            case Bytecodes::_iconst_m1:		gen(iconst(-1),     vtos, itos);
            case Bytecodes::_iconst_1:		gen(iconst(1),      vtos, itos);
            case Bytecodes::_iconst_2:		gen(iconst(2),		vtos, itos);
            case Bytecodes::_iconst_3:		gen(iconst(3),		vtos, itos);
            case Bytecodes::_iconst_4:		gen(iconst(4),		vtos, itos);
            case Bytecodes::_iconst_5:		gen(iconst(5),		vtos, itos);
            case Bytecodes::_lconst_0:		gen(lconst(0),		vtos, ltos);
            case Bytecodes::_lconst_1:		gen(lconst(1),		vtos, ltos);
            case Bytecodes::_fconst_0:		gen(fconst(0),		vtos, ftos);
            case Bytecodes::_fconst_1:		gen(fconst(1),		vtos, ftos);
            case Bytecodes::_fconst_2:		gen(fconst(2),		vtos, ftos);
            case Bytecodes::_dconst_0:		gen(dconst(0),		vtos, dtos);
            case Bytecodes::_dconst_1:		gen(dconst(1),		vtos, dtos);
            case Bytecodes::_bipush:		gen(bipush(),		vtos, itos);
            case Bytecodes::_sipush:		gen(sipush(),		vtos, itos);
            case Bytecodes::_ldc:			gen(ldc(false),		vtos, vtos);
            case Bytecodes::_ldc_w:			gen(ldc(true),		vtos, vtos);
            case Bytecodes::_ldc2_w:		gen(ldc2_w(),		vtos, vtos);
            case Bytecodes::_iload:			gen(iload(),		vtos, itos);
            case Bytecodes::_lload:			gen(lload(),		vtos, ltos);
            case Bytecodes::_fload:			gen(fload(),		vtos, ftos);
            case Bytecodes::_dload:			gen(dload(),		vtos, dtos);
            case Bytecodes::_aload:			gen(aload(),		vtos, atos);
            case Bytecodes::_iload_0:		gen(iload(0),		vtos, itos);
            case Bytecodes::_iload_1:		gen(iload(1),		vtos, itos);
            case Bytecodes::_iload_2:		gen(iload(2),		vtos, itos);
            case Bytecodes::_iload_3:		gen(iload(3),		vtos, itos);
            case Bytecodes::_lload_0:		gen(lload(0),		vtos, ltos);
            case Bytecodes::_lload_1:		gen(lload(1),		vtos, ltos);
            case Bytecodes::_lload_2:		gen(lload(2),		vtos, ltos);
            case Bytecodes::_lload_3:		gen(lload(3),		vtos, ltos);
            case Bytecodes::_fload_0:		gen(fload(0),		vtos, ftos);
            case Bytecodes::_fload_1:		gen(fload(1),		vtos, ftos);
            case Bytecodes::_fload_2:		gen(fload(2),		vtos, ftos);
            case Bytecodes::_fload_3:		gen(fload(3),		vtos, ftos);
            case Bytecodes::_dload_0:		gen(dload(0),		vtos, dtos);
            case Bytecodes::_dload_1:		gen(dload(1),		vtos, dtos);
            case Bytecodes::_dload_2:		gen(dload(2),		vtos, dtos);
            case Bytecodes::_dload_3:		gen(dload(3),		vtos, dtos);
            case Bytecodes::_aload_0:		gen(aload(0),		vtos, atos);
            case Bytecodes::_aload_1:		gen(aload(1),		vtos, atos);
            case Bytecodes::_aload_2:		gen(aload(2),		vtos, atos);
            case Bytecodes::_aload_3:		gen(aload(3),		vtos, atos);
            case Bytecodes::_iaload:		gen(iaload(),		itos, itos);
            case Bytecodes::_laload:		gen(laload(),		itos, ltos);
            case Bytecodes::_faload:		gen(faload(),		itos, ftos);
            case Bytecodes::_daload:		gen(daload(),		itos, dtos);
            case Bytecodes::_aaload:		gen(aaload(),		itos, atos);
            case Bytecodes::_baload:		gen(baload(),		itos, btos);
            case Bytecodes::_caload:		gen(caload(),		itos, ctos);
            case Bytecodes::_saload:		gen(saload(),		itos, stos);
            case Bytecodes::_istore:		gen(istore(),		itos, vtos);
            case Bytecodes::_lstore:		gen(lstore(),		ltos, vtos);
            case Bytecodes::_fstore:		gen(fstore(),		ftos, vtos);
            case Bytecodes::_dstore:		gen(dstore(),		dtos, vtos);
            case Bytecodes::_astore:		gen(astore(),		atos, vtos);
            case Bytecodes::_istore_0:		gen(istore(0),		itos, vtos);
            case Bytecodes::_istore_1:		gen(istore(1),		itos, vtos);
            case Bytecodes::_istore_2:		gen(istore(2),		itos, vtos);
            case Bytecodes::_istore_3:		gen(istore(3),		itos, vtos);
            case Bytecodes::_lstore_0:		gen(lstore(0),		ltos, vtos);
            case Bytecodes::_lstore_1:		gen(lstore(1),		ltos, vtos);
            case Bytecodes::_lstore_2:		gen(lstore(2),		ltos, vtos);
            case Bytecodes::_lstore_3:		gen(lstore(3),		ltos, vtos);
            case Bytecodes::_fstore_0:		gen(fstore(0),		ftos, vtos);
            case Bytecodes::_fstore_1:		gen(fstore(1),		ftos, vtos);
            case Bytecodes::_fstore_2:		gen(fstore(2),		ftos, vtos);
            case Bytecodes::_fstore_3:		gen(fstore(3),		ftos, vtos);
            case Bytecodes::_dstore_0:		gen(dstore(0),		dtos, vtos);
            case Bytecodes::_dstore_1:		gen(dstore(1),		dtos, vtos);
            case Bytecodes::_dstore_2:		gen(dstore(2),		dtos, vtos);
            case Bytecodes::_dstore_3:		gen(dstore(3),		dtos, vtos);
            case Bytecodes::_astore_0:		gen(astore(0),		atos, vtos);
            case Bytecodes::_astore_1:		gen(astore(1),		atos, vtos);
            case Bytecodes::_astore_2:		gen(astore(2),		atos, vtos);
            case Bytecodes::_astore_3:		gen(astore(3),		atos, vtos);
            case Bytecodes::_iastore:		gen(iastore(),		itos, vtos);
            case Bytecodes::_lastore:		gen(lastore(),		ltos, vtos);
            case Bytecodes::_fastore:		gen(fastore(),		ftos, vtos);
            case Bytecodes::_dastore:		gen(dastore(),		dtos, vtos);
            case Bytecodes::_aastore:		gen(aastore(),		vtos, vtos);
            case Bytecodes::_bastore:		gen(bastore(),		btos, vtos);
            case Bytecodes::_castore:		gen(castore(),		ctos, vtos);
            case Bytecodes::_sastore:		gen(sastore(),		stos, vtos);
            case Bytecodes::_pop:			gen(pop(),		    vtos, vtos);
            case Bytecodes::_pop2:			gen(pop2(),		    vtos, vtos);
            case Bytecodes::_dup:			gen(dup(),		    vtos, vtos);
            case Bytecodes::_dup_x1:		gen(dup_x1(),		vtos, vtos);
            case Bytecodes::_dup_x2:		gen(dup_x2(),		vtos, vtos);
            case Bytecodes::_dup2:			gen(dup2(),		    vtos, vtos);
            case Bytecodes::_dup2_x1:		gen(dup2_x1(),		vtos, vtos);
            case Bytecodes::_dup2_x2:		gen(dup2_x2(),		vtos, vtos);
            case Bytecodes::_swap:			gen(swap(),		    vtos, vtos);
            case Bytecodes::_iadd:			gen(iop2(add),		itos, itos);
            case Bytecodes::_ladd:			gen(lop2(add),		ltos, ltos);
            case Bytecodes::_fadd:			gen(fop2(add),		ftos, ftos);
            case Bytecodes::_dadd:			gen(dop2(add),		dtos, dtos);
            case Bytecodes::_isub:			gen(iop2(sub),		itos, itos);
            case Bytecodes::_lsub:			gen(lop2(sub),		ltos, ltos);
            case Bytecodes::_fsub:			gen(fop2(sub),		ftos, ftos);
            case Bytecodes::_dsub:			gen(dop2(sub),		dtos, dtos);
            case Bytecodes::_imul:			gen(iop2(mul),		itos, itos);
            case Bytecodes::_lmul:			gen(lmul(),		    ltos, ltos);
            case Bytecodes::_fmul:			gen(fop2(mul),		ftos, ftos);
            case Bytecodes::_dmul:			gen(dop2(mul),		dtos, dtos);
            case Bytecodes::_idiv:			gen(idiv(),		    itos, itos);
            case Bytecodes::_ldiv:			gen(ldiv(),		    ltos, ltos);
            case Bytecodes::_fdiv:			gen(fop2(div),		ftos, ftos);
            case Bytecodes::_ddiv:			gen(dop2(div),		dtos, dtos);
            case Bytecodes::_irem:			gen(irem(),		    itos, itos);
            case Bytecodes::_lrem:			gen(lrem(),		    ltos, ltos);
            case Bytecodes::_frem:			gen(fop2(rem),		ftos, ftos);
            case Bytecodes::_drem:			gen(dop2(rem),		dtos, dtos);
            case Bytecodes::_ineg:			gen(ineg(),		    itos, itos);
            case Bytecodes::_lneg:			gen(lneg(),		    ltos, ltos);
            case Bytecodes::_fneg:			gen(fneg(),		    ftos, ftos);
            case Bytecodes::_dneg:			gen(dneg(),		    dtos, dtos);
            case Bytecodes::_ishl:			gen(iop2(shl),		itos, itos);
            case Bytecodes::_lshl:			gen(lshl(),		    itos, ltos);
            case Bytecodes::_ishr:			gen(iop2(shr),		itos, itos);
            case Bytecodes::_lshr:			gen(lshr(),		    itos, ltos);
            case Bytecodes::_iushr:			gen(iop2(ushr),		itos, itos);
            case Bytecodes::_lushr:			gen(lushr(),		itos, ltos);
            case Bytecodes::_iand:			gen(iop2(_and),		itos, itos);
            case Bytecodes::_land:			gen(lop2(_and),		ltos, ltos);
            case Bytecodes::_ior:			gen(iop2(_or),		itos, itos);
            case Bytecodes::_lor:			gen(lop2(_or),		ltos, ltos);
            case Bytecodes::_ixor:			gen(iop2(_xor),		itos, itos);
            case Bytecodes::_lxor:			gen(lop2(_xor),		ltos, ltos);
            case Bytecodes::_iinc:			gen(iinc(),		    vtos, vtos);
            case Bytecodes::_i2l:			gen(convert(),		itos, ltos);
            case Bytecodes::_i2f:			gen(convert(),		itos, ftos);
            case Bytecodes::_i2d:			gen(convert(),		itos, dtos);
            case Bytecodes::_l2i:			gen(convert(),		ltos, itos);
            case Bytecodes::_l2f:			gen(convert(),		ltos, ftos);
            case Bytecodes::_l2d:			gen(convert(),		ltos, dtos);
            case Bytecodes::_f2i:			gen(convert(),		ftos, itos);
            case Bytecodes::_f2l:			gen(convert(),		ftos, ltos);
            case Bytecodes::_f2d:			gen(convert(),		ftos, dtos);
            case Bytecodes::_d2i:			gen(convert(),		dtos, itos);
            case Bytecodes::_d2l:			gen(convert(),		dtos, ltos);
            case Bytecodes::_d2f:			gen(convert(),		dtos, ftos);
            case Bytecodes::_i2b:			gen(convert(),		itos, btos);
            case Bytecodes::_i2c:			gen(convert(),		itos, ctos);
            case Bytecodes::_i2s:			gen(convert(),		itos, stos);
            case Bytecodes::_lcmp:			gen(lcmp(),		    ltos, itos);
            case Bytecodes::_fcmpl:			gen(float_cmp(-1),	ftos, itos);
            case Bytecodes::_fcmpg:			gen(float_cmp(1),	ftos, itos);
            case Bytecodes::_dcmpl:			gen(double_cmp(-1),	dtos, itos);
            case Bytecodes::_dcmpg:			gen(double_cmp(1),	dtos, itos);
            case Bytecodes::_ifeq:			gen(if_0cmp(equal),	        itos, vtos);
            case Bytecodes::_ifne:			gen(if_0cmp(not_equal),	    itos, vtos);
            case Bytecodes::_iflt:			gen(if_0cmp(less),		    itos, vtos);
            case Bytecodes::_ifge:			gen(if_0cmp(greater_equal),  itos, vtos);
            case Bytecodes::_ifgt:			gen(if_0cmp(greater),		itos, vtos);
            case Bytecodes::_ifle:			gen(if_0cmp(less_equal),	    itos, vtos);
            case Bytecodes::_if_icmpeq:		gen(if_icmp(equal),		    itos, vtos);
            case Bytecodes::_if_icmpne:		gen(if_icmp(not_equal),		itos, vtos);
            case Bytecodes::_if_icmplt:		gen(if_icmp(less),		    itos, vtos);
            case Bytecodes::_if_icmpge:		gen(if_icmp(greater_equal),	itos, vtos);
            case Bytecodes::_if_icmpgt:		gen(if_icmp(greater),		itos, vtos);
            case Bytecodes::_if_icmple:		gen(if_icmp(less_equal),	    itos, vtos);
            case Bytecodes::_if_acmpeq:		gen(if_acmp(equal),		    atos, vtos);
            case Bytecodes::_if_acmpne:		gen(if_acmp(not_equal),		atos, vtos);
            case Bytecodes::_goto:			gen(_goto(),		vtos, vtos);
            case Bytecodes::_jsr:			gen(jsr(),		    vtos, vtos);
            case Bytecodes::_ret:			gen(ret(),		    vtos, vtos);
            case Bytecodes::_tableswitch:   gen(tableswitch(),          itos, vtos);
            case Bytecodes::_lookupswitch:  gen(lookupswitch(),         itos, vtos);
            case Bytecodes::_ireturn:       gen(_jmp_return(),          itos, vtos);
            case Bytecodes::_lreturn:       gen(_jmp_return(),          ltos, vtos);
            case Bytecodes::_freturn:       gen(_jmp_return(),          ftos, vtos);
            case Bytecodes::_dreturn:       gen(_jmp_return(),          dtos, vtos);
            case Bytecodes::_areturn:       gen(_jmp_return(),          atos, vtos);
            case Bytecodes::_return:        gen(_jmp_return(),          vtos, vtos);
            case Bytecodes::_getstatic:     gen(getstatic(f1_byte), vtos, vtos);
            case Bytecodes::_putstatic:     gen(putstatic(f2_byte), vtos, vtos);
            case Bytecodes::_getfield:      gen(getfield(f1_byte),  vtos, vtos);
            case Bytecodes::_putfield:      gen(putfield(f2_byte),  vtos, vtos);
            case Bytecodes::_invokevirtual: gen(invokevirtual(f2_byte), vtos, vtos);
            case Bytecodes::_invokespecial: gen(invokevirtual(f1_byte), vtos, vtos);
            case Bytecodes::_invokestatic:  gen(invokestatic(f1_byte),  vtos, vtos);
            case Bytecodes::_invokeinterface:gen(invokeinterface(f1_byte), vtos, vtos);
            case Bytecodes::_invokedynamic: gen(invokedynamic(f1_byte), vtos, vtos);
            case Bytecodes::_new:           gen(_new(),             vtos, atos);
            case Bytecodes::_newarray:      gen(newarray(),         itos, atos);
            case Bytecodes::_anewarray:     gen(anewarray(),        itos, atos);
            case Bytecodes::_arraylength:   gen(arraylength(),      atos, itos);
            case Bytecodes::_athrow:        gen(athrow(),           atos, vtos);
            case Bytecodes::_checkcast:     gen(checkcast(),        atos, atos);
            case Bytecodes::_instanceof:    gen(instanceof(),       atos, itos);
            //case Bytecodes::_monitorenter:  gen(monitorenter(),     atos, vtos);
            //case Bytecodes::_monitorexit:   gen(monitorexit(),      atos, vtos);
            case Bytecodes::_wide:
                Unimplemented();
                break;
            case Bytecodes::_multianewarray:gen(multianewarray(),   vtos, atos);
            case Bytecodes::_ifnull:        gen(if_nullcmp(equal),  atos, vtos);
            case Bytecodes::_ifnonnull:     gen(if_nullcmp(not_equal),  atos, vtos);
            case Bytecodes::_goto_w:        gen(goto_w(),           vtos, vtos);
            case Bytecodes::_jsr_w:         gen(jsr_w(),            vtos, vtos);
            case Bytecodes::_breakpoint:
                Unimplemented();
                break;
            default:
                Unimplemented();
        }
    }

    __ end_expression_stack();

    TosState ret_tos = MetadataAccessor::basicType2tosState(method->result_type());
    __ bind(ret_now);
    _return(ret_tos);
    for (std::vector<std::pair<int, address> >::iterator it = patch_address.begin();
            it != patch_address.end();++it) {
        int bci = it->first;
        address jmp_addr = bci_ptr_map[bci];
        if (jmp_addr == NULL) ShouldNotReachHere();
        patch_jmp(it->second, jmp_addr);
    }

    // patch the dispatch table for tabledispatch
    for (std::vector<std::pair<int, address> >::iterator it = patch_dispatch.begin();
         it != patch_dispatch.end();++it) {
        int bci = it->first;
        intptr_t jmp_addr = (intptr_t)bci_ptr_map[bci];
        if (jmp_addr == NULL) ShouldNotReachHere();
        intptr_t* patch_site = (intptr_t*)it->second;
        *patch_site = jmp_addr;
    }

    for (std::deque<CodeStub *>::iterator itr = slow_cases.begin(); itr != slow_cases.end();itr++) {
        (*itr)->emit();
    }

//    for (std::deque<CodeStub *>::iterator itr = slow_cases.begin(); itr != slow_cases.end();itr++) {
//        delete (PatchingStub*)(*itr);
//    }

    if (has_interface) {
      __ bind(no_such_interface);
      //abort
      // movptr change to str
      __ str(r0, Address(r0, -1));
      __ str(r0, Address(r0, 0));
    }

    __ flush();

    int offset = __ offset();

    if (size == 0) {
        BufferBlob::free(stub_blob);
        delete oopSet;
    }
    return offset;
//    oopSet->print();

//    for (std::map<int, TosState>::iterator it = jmp_target.begin();
//        it != jmp_target.end();++it) {
//        int bcii = it->first;
//        if (strcmp(method->name()->as_C_string(), "escape") == 0) {
//            printf("bcii %d\n", bcii);
//        }
//
//        std::map<int, TosState>::iterator map_it = bci_tos.find(it->first);
//        if (map_it != bci_tos.end()) {
//          if (map_it->second != it->second) {
//            printf("inconsistent in %s:%d, expected %d, got %d\n", method->name()->as_C_string(), it->first, it->second, bci_tos[it->first]);
//          }
//        } else {
//          printf("can not find\n");
//        }
//    }
}

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
    __ load_signed_byte32(r0, at_bcp(1));
}
void NormalCompileTask::sipush() {
  transition(vtos, itos);
  __ load_unsigned_short(r0, at_bcp(1));
  __ revw(r0, r0);
  __ asrw(r0, r0, 16);
}
void NormalCompileTask::ldc(bool wide) {
  transition(vtos, vtos);
  Label call_ldc, notFloat, notClass, Done;

  if (wide) {
    __ get_unsigned_2_byte_index_at_bcp(r1, 1);
  } else {
    __ load_unsigned_byte(r1, at_bcp(1));
  }
  __ get_cpool_and_tags(r2, r0);

  const int base_offset = ConstantPool::header_size() * wordSize;
  const int tags_offset = Array<u1>::base_offset_in_bytes();

  // get type
  __ add(r3, r1, tags_offset);
  __ lea(r3, Address(r0, r3));
  __ ldarb(r3, r3);

  // unresolved class - get the resolved class
  __ cmp(r3, JVM_CONSTANT_UnresolvedClass);
  __ br(Assembler::EQ, call_ldc);

  // unresolved class in error state - call into runtime to throw the error
  // from the first resolution attempt
  __ cmp(r3, JVM_CONSTANT_UnresolvedClassInError);
  __ br(Assembler::EQ, call_ldc);

  // resolved class - need to call vm to get java mirror of the class
  __ cmp(r3, JVM_CONSTANT_Class);
  __ br(Assembler::NE, notClass);

  __ bind(call_ldc);
  __ mov(c_rarg1, wide);
  __ call_VM(r0, CAST_FROM_FN_PTR(address, InterpreterRuntime::ldc), c_rarg1);
  __ push_ptr(r0);
  __ verify_oop(r0);
  __ b(Done);

  __ bind(notClass);
  __ cmp(r3, JVM_CONSTANT_Float);
  __ br(Assembler::NE, notFloat);
  // ftos
  __ adds(r1, r2, r1, Assembler::LSL, 3);
  __ ldrs(v0, Address(r1, base_offset));
  __ push_f();
  __ b(Done);

  __ bind(notFloat);
#ifdef ASSERT
  {
    Label L;
    __ cmp(r3, JVM_CONSTANT_Integer);
    __ br(Assembler::EQ, L);
    // String and Object are rewritten to fast_aldc
    __ stop("unexpected tag type in ldc");
    __ bind(L);
  }
#endif
  // itos JVM_CONSTANT_Integer only
  __ adds(r1, r2, r1, Assembler::LSL, 3);
  __ ldrw(r0, Address(r1, base_offset));
  __ push_i(r0);
  __ bind(Done);    
}

void NormalCompileTask::ldc2_w() {
  transition(vtos, vtos);
  Label Long, Done;
  __ get_unsigned_2_byte_index_at_bcp(r0, 1);

  __ get_cpool_and_tags(r1, r2);
  const int base_offset = ConstantPool::header_size() * wordSize;
  const int tags_offset = Array<u1>::base_offset_in_bytes();

  // get type
  __ lea(r2, Address(r2, r0, Address::lsl(0)));
  __ load_unsigned_byte(r2, Address(r2, tags_offset));
  __ cmpw(r2, (int)JVM_CONSTANT_Double);
  __ br(Assembler::NE, Long);
  // dtos
  __ lea (r2, Address(r1, r0, Address::lsl(3)));
  __ ldrd(v0, Address(r2, base_offset));
  __ push_d();
  __ b(Done);

  __ bind(Long);
  // ltos
  __ lea(r0, Address(r1, r0, Address::lsl(3)));
  __ ldr(r0, Address(r0, base_offset));
  __ push_l();

  __ bind(Done);
}
void NormalCompileTask::iload() {
  transition(vtos, itos);
  if (RewriteFrequentPairs) {
    Label rewrite, done;
    Register bc = r4;

    // get next bytecode
    __ load_unsigned_byte(r1, at_bcp(Bytecodes::length_for(Bytecodes::_iload)));

    // if _iload, wait to rewrite to iload2.  We only want to rewrite the
    // last two iloads in a pair.  Comparing against fast_iload means that
    // the next bytecode is neither an iload or a caload, and therefore
    // an iload pair.
    __ cmpw(r1, Bytecodes::_iload);
    __ br(Assembler::EQ, done);

    // if _fast_iload rewrite to _fast_iload2
    __ cmpw(r1, Bytecodes::_fast_iload);
    __ movw(bc, Bytecodes::_fast_iload2);
    __ br(Assembler::EQ, rewrite);

    // if _caload rewrite to _fast_icaload
    __ cmpw(r1, Bytecodes::_caload);
    __ movw(bc, Bytecodes::_fast_icaload);
    __ br(Assembler::EQ, rewrite);

    // else rewrite to _fast_iload
    __ movw(bc, Bytecodes::_fast_iload);

    // rewrite
    // bc: new bytecode
    __ bind(rewrite);
      //fill with 0
    patch_bytecode(Bytecodes::_iload, bc, r1, false, 0);
    __ bind(done);

  }

  // do iload, get the local value into tos
  locals_index(r1);
  __ ldr(r0, iaddress(r1));

}
void NormalCompileTask::iload(int n) {
    transition(vtos, itos);
    __ ldr(r0, iaddress(n));
}
void NormalCompileTask::lload() {
  transition(vtos, ltos);
  __ ldrb(r1, at_bcp(1));
  __ sub(r1, rlocals, r1, ext::uxtw, LogBytesPerWord);
  __ ldr(r0, Address(r1, Interpreter::local_offset_in_bytes(1)));
}
void NormalCompileTask::lload(int n) {
  transition(vtos, ltos);
  __ ldr(r0, laddress(n));
}
void NormalCompileTask::fload() {
  transition(vtos, ftos);
  locals_index(r1);
  // n.b. we use ldrd here because this is a 64 bit slot
  // this is comparable to the iload case
  __ ldrd(v0, faddress(r1));
}
void NormalCompileTask::fload(int n) {
  transition(vtos, ftos);
  __ ldrs(v0, faddress(n));
}
void NormalCompileTask::dload() {
  transition(vtos, dtos);
  __ ldrb(r1, at_bcp(1));
  __ sub(r1, rlocals, r1, ext::uxtw, LogBytesPerWord);
  __ ldrd(v0, Address(r1, Interpreter::local_offset_in_bytes(1)));
}
void NormalCompileTask::dload(int n) {
  transition(vtos, dtos);
  __ ldrd(v0, daddress(n));
}
void NormalCompileTask::aload() {
  transition(vtos, atos);
  locals_index(r1);
  __ ldr(r0, iaddress(r1));
}
void NormalCompileTask::aload(int n) {
  transition(vtos, atos);
  __ ldr(r0, iaddress(n));
}
void NormalCompileTask::aload_0() {
  // According to bytecode histograms, the pairs:
  //
  // _aload_0, _fast_igetfield
  // _aload_0, _fast_agetfield
  // _aload_0, _fast_fgetfield
  //
  // occur frequently. If RewriteFrequentPairs is set, the (slow)
  // _aload_0 bytecode checks if the next bytecode is either
  // _fast_igetfield, _fast_agetfield or _fast_fgetfield and then
  // rewrites the current bytecode into a pair bytecode; otherwise it
  // rewrites the current bytecode into _fast_aload_0 that doesn't do
  // the pair check anymore.
  //
  // Note: If the next bytecode is _getfield, the rewrite must be
  //       delayed, otherwise we may miss an opportunity for a pair.
  //
  // Also rewrite frequent pairs
  //   aload_0, aload_1
  //   aload_0, iload_1
  // These bytecodes with a small amount of code are most profitable
  // to rewrite
  if (RewriteFrequentPairs) {
    Label rewrite, done;
    const Register bc = r4;

    // get next bytecode
    __ load_unsigned_byte(r1, at_bcp(Bytecodes::length_for(Bytecodes::_aload_0)));

    // do actual aload_0
    aload(0);

    // if _getfield then wait with rewrite
    __ cmpw(r1, Bytecodes::Bytecodes::_getfield);
    __ br(Assembler::EQ, done);

    // if _igetfield then reqrite to _fast_iaccess_0
    assert(Bytecodes::java_code(Bytecodes::_fast_iaccess_0) == Bytecodes::_aload_0, "fix bytecode definition");
    __ cmpw(r1, Bytecodes::_fast_igetfield);
    __ movw(bc, Bytecodes::_fast_iaccess_0);
    __ br(Assembler::EQ, rewrite);

    // if _agetfield then reqrite to _fast_aaccess_0
    assert(Bytecodes::java_code(Bytecodes::_fast_aaccess_0) == Bytecodes::_aload_0, "fix bytecode definition");
    __ cmpw(r1, Bytecodes::_fast_agetfield);
    __ movw(bc, Bytecodes::_fast_aaccess_0);
    __ br(Assembler::EQ, rewrite);

    // if _fgetfield then reqrite to _fast_faccess_0
    assert(Bytecodes::java_code(Bytecodes::_fast_faccess_0) == Bytecodes::_aload_0, "fix bytecode definition");
    __ cmpw(r1, Bytecodes::_fast_fgetfield);
    __ movw(bc, Bytecodes::_fast_faccess_0);
    __ br(Assembler::EQ, rewrite);

    // else rewrite to _fast_aload0
    assert(Bytecodes::java_code(Bytecodes::_fast_aload_0) == Bytecodes::_aload_0, "fix bytecode definition");
    __ movw(bc, Bytecodes::Bytecodes::_fast_aload_0);

    // rewrite
    // bc: new bytecode
    __ bind(rewrite);
      //fill with 0
    patch_bytecode(Bytecodes::_aload_0, bc, r1, false, 0);

    __ bind(done);
  } else {
    aload(0);
  }
}

void NormalCompileTask::iaload() {
  transition(itos, itos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  //oopDesc::bs()->interpreter_read_barrier_not_null(_masm, r0);
  __ lea(r1, Address(r0, r1, Address::uxtw(2)));
  __ ldrw(r0, Address(r1, arrayOopDesc::base_offset_in_bytes(T_INT)));
}
void NormalCompileTask::laload() {
  transition(itos, ltos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  //oopDesc::bs()->interpreter_read_barrier_not_null(_masm, r0);
  __ lea(r1, Address(r0, r1, Address::uxtw(3)));
  __ ldr(r0, Address(r1,  arrayOopDesc::base_offset_in_bytes(T_LONG)));
}
void NormalCompileTask::faload() {
  transition(itos, ftos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  //oopDesc::bs()->interpreter_read_barrier_not_null(_masm, r0);
  __ lea(r1,  Address(r0, r1, Address::uxtw(2)));
  __ ldrs(v0, Address(r1,  arrayOopDesc::base_offset_in_bytes(T_FLOAT)));
}
void NormalCompileTask::daload() {
  transition(itos, dtos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  //oopDesc::bs()->interpreter_read_barrier_not_null(_masm, r0);
  __ lea(r1,  Address(r0, r1, Address::uxtw(3)));
  __ ldrd(v0, Address(r1,  arrayOopDesc::base_offset_in_bytes(T_DOUBLE)));

}
void NormalCompileTask::aaload() {
  transition(itos, atos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  //oopDesc::bs()->interpreter_read_barrier_not_null(_masm, r0);
  int s = (UseCompressedOops ? 2 : 3);
  __ lea(r1, Address(r0, r1, Address::uxtw(s)));
  __ load_heap_oop(r0, Address(r1, arrayOopDesc::base_offset_in_bytes(T_OBJECT)));
}
void NormalCompileTask::baload() {
  transition(itos, itos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  //oopDesc::bs()->interpreter_read_barrier_not_null(_masm, r0);
  __ lea(r1,  Address(r0, r1, Address::uxtw(0)));
  __ load_signed_byte(r0, Address(r1,  arrayOopDesc::base_offset_in_bytes(T_BYTE)));
}
void NormalCompileTask::caload() {
  transition(itos, itos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  //oopDesc::bs()->interpreter_read_barrier_not_null(_masm, r0);
  __ lea(r1,  Address(r0, r1, Address::uxtw(1)));
  __ load_unsigned_short(r0, Address(r1,  arrayOopDesc::base_offset_in_bytes(T_CHAR)));
}
void NormalCompileTask::saload() {
  transition(itos, itos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  //oopDesc::bs()->interpreter_read_barrier_not_null(_masm, r0);
  __ lea(r1,  Address(r0, r1, Address::uxtw(1)));
  __ load_signed_short(r0, Address(r1,  arrayOopDesc::base_offset_in_bytes(T_SHORT)));
}
void NormalCompileTask::istore(){
  transition(itos, vtos);
  locals_index(r1);
  // FIXME: We're being very pernickerty here storing a jint in a
  // local with strw, which costs an extra instruction over what we'd
  // be able to do with a simple str.  We should just store the whole
  // word.
  __ lea(rscratch1, iaddress(r1));
  __ strw(r0, Address(rscratch1));
}
void NormalCompileTask::istore(int n){
  transition(itos, vtos);
  __ str(r0, iaddress(n));
}
void NormalCompileTask::lstore(){
  transition(ltos, vtos);
  locals_index(r1);
  __ str(r0, laddress(r1, rscratch1, _masm));
}
void NormalCompileTask::lstore(int n){
  transition(ltos, vtos);
  __ str(r0, laddress(n));
}
void NormalCompileTask::fstore(){
  transition(ftos, vtos);
  locals_index(r1);
  __ lea(rscratch1, iaddress(r1));
  __ strs(v0, Address(rscratch1));
}
void NormalCompileTask::fstore(int n){
  transition(ftos, vtos);
  __ strs(v0, faddress(n));
}
void NormalCompileTask::dstore(){
  transition(dtos, vtos);
  locals_index(r1);
  __ strd(v0, daddress(r1, rscratch1, _masm));
}
void NormalCompileTask::dstore(int n){
  transition(dtos, vtos);
  __ strd(v0, daddress(n));
}
void NormalCompileTask::astore(){
  transition(vtos, vtos);
  __ pop_ptr(r0);
  locals_index(r1);
  __ str(r0, aaddress(r1));
}
void NormalCompileTask::astore(int n){
  transition(vtos, vtos);
  __ pop_ptr(r0);
  __ str(r0, iaddress(n));
}
void NormalCompileTask::iastore() {
  transition(itos, vtos);
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
  __ add(esp, esp, Interpreter::stackElementSize);
}
void NormalCompileTask::pop2() {
  transition(vtos, vtos);
  __ add(esp, esp, 2 * Interpreter::stackElementSize);
}
void NormalCompileTask::dup() {
  transition(vtos, vtos);
  __ ldr(r0, Address(esp, 0));
  __ push(r0);
  // stack: ..., a, a
}
void NormalCompileTask::dup_x1() {
  transition(vtos, vtos);
  // stack: ..., a, b
  __ ldr(r0, at_tos());  // load b
  __ ldr(r2, at_tos_p1());  // load a
  __ str(r0, at_tos_p1());  // store b
  __ str(r2, at_tos());  // store a
  __ push(r0);			// push b
  // stack: ..., b, a, b
}
void NormalCompileTask::dup_x2() {
  transition(vtos, vtos);
  // stack: ..., a, b, c
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
  __ ldr(r0, at_tos_p1());  // load a
  __ push(r0);                  // push a
  __ ldr(r0, at_tos_p1());  // load b
  __ push(r0);                  // push b
  // stack: ..., a, b, a, b
}
void NormalCompileTask::dup2_x1(){
  transition(vtos, vtos);
  // stack: ..., a, b, c
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
  __ ldr(r2, at_tos_p1());  // load a
  __ ldr(r0, at_tos());  // load b
  __ str(r2, at_tos());  // store a in b
  __ str(r0, at_tos_p1());  // store b in a
  // stack: ..., b, a
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
  __ corrected_idivl(r0, r1, r0, /* want_remainder */ true);
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
  __ corrected_idivq(r0, r1, r0, /* want_remainder */ true);
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
  else if (cc == not_equal)
    __ cbzw(r0, not_taken);
  else {
    __ andsw(zr, r0, r0);
    __ br(j_not(cc), not_taken);
  }

  branch(false, false);
  __ bind(not_taken);
  __ profile_not_taken_branch(r0);
}
void NormalCompileTask::if_icmp(NormalCompileTask::Condition cc) {
  transition(itos, vtos);
  // assume branch is more often taken than not (loops use backward branches)
  Label not_taken;
  __ pop_i(r1);
  __ cmpw(r1, r0, Assembler::LSL);
  __ br(j_not(cc), not_taken);
  branch(false, false);
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
  branch(false, false);
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
  branch(false, false);
  __ bind(not_taken);
  __ profile_not_taken_branch(r0);
}
//not found in aarch64 template table
void NormalCompileTask::_goto() {
    branch(false, false);
    tos = udtos;
}
void NormalCompileTask::jsr(){
    Unimplemented();
}
void NormalCompileTask::ret(){
  transition(vtos, vtos);
  // We might be moving to a safepoint.  The thread which calls
  // Interpreter::notice_safepoints() will effectively flush its cache
  // when it makes a system call, but we need to do something to
  // ensure that we see the changed dispatch table.
  __ membar(MacroAssembler::LoadLoad);

  locals_index(r1);
  __ ldr(r1, aaddress(r1)); // get return bci, compute return bcp
  __ profile_ret(r1, r2);
  __ ldr(rbcp, Address(rmethod, Method::const_offset()));
  __ lea(rbcp, Address(rbcp, r1));
  __ add(rbcp, rbcp, in_bytes(ConstMethod::codes_offset()));
  __ dispatch_next(vtos);
}
void NormalCompileTask::iinc(){
  transition(vtos, vtos);
  __ load_signed_byte(r1, at_bcp(2)); // get constant
  locals_index(r2);
  __ ldr(r0, iaddress(r2));
  __ addw(r0, r0, r1);
  __ str(r0, iaddress(r2));
}
void NormalCompileTask::wide_iinc() {
  transition(vtos, vtos);
  // __ mov(r1, zr);
  __ ldrw(r1, at_bcp(2)); // get constant and index
  __ rev16(r1, r1);
  __ ubfx(r2, r1, 0, 16);
  __ neg(r2, r2);
  __ sbfx(r1, r1, 16, 16);
  __ ldr(r0, iaddress(r2));
  __ addw(r0, r0, r1);
  __ str(r0, iaddress(r2));
}
void NormalCompileTask::branch(bool is_jsr, bool is_wide){
    // We might be moving to a safepoint.  The thread which calls
    // Interpreter::notice_safepoints() will effectively flush its cache
    // when it makes a system call, but we need to do something to
    // ensure that we see the changed dispatch table.
    __ membar(MacroAssembler::LoadLoad);

    __ profile_taken_branch(r0, r1);

    // load branch displacement
    if (!is_wide) {
        __ ldrh(r2, at_bcp(1));
        __ rev16(r2, r2);
        // sign extend the 16 bit value in r2
        __ sbfm(r2, r2, 0, 15);
    } else {
        __ ldrw(r2, at_bcp(1));
        __ revw(r2, r2);
        // sign extend the 32 bit value in r2
        __ sbfm(r2, r2, 0, 31);
    }

    // Handle all the JSR stuff here, then exit.
    // It's much shorter and cleaner than intermingling with the non-JSR
    // normal-branch stuff occurring below.

    if (is_jsr) {
        // Pre-load the next target bytecode into rscratch1
        __ load_unsigned_byte(rscratch1, Address(rbcp, r2));
        // compute return address as bci
        __ ldr(rscratch2, Address(rmethod, Method::const_offset()));
        __ add(rscratch2, rscratch2,
               in_bytes(ConstMethod::codes_offset()) - (is_wide ? 5 : 3));
        __ sub(r1, rbcp, rscratch2);
        __ push_i(r1);
        // Adjust the bcp by the 16-bit displacement in r2
        __ add(rbcp, rbcp, r2);
        __ dispatch_only(vtos);
        return;
    }

    // Normal (non-jsr) branch handling

    // Adjust the bcp by the displacement in r2
    __ add(rbcp, rbcp, r2);

    assert(UseLoopCounter || !UseOnStackReplacement,
           "on-stack-replacement requires loop counters");
    Label backedge_counter_overflow;
    Label profile_method;
    Label dispatch;

    __ bind(dispatch);

    // Pre-load the next target bytecode into rscratch1
    __ load_unsigned_byte(rscratch1, Address(rbcp, 0));

    // continue with the bytecode @ target
    // rscratch1: target bytecode
    // rbcp: target bcp
    __ dispatch_only(vtos);
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
  __ corrected_idivq(r0, r1, r0, /* want_remainder */ false);
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
  __ corrected_idivl(r0, r1, r0, /* want_remainder */ false);
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
    //Comment out because it is for debug and it is for x86
      //__ push(r11);
      //__ mov(c_rarg0, r15_thread);
      //__ mov(c_rarg1, r13);
      //__ movptr(c_rarg2, (intptr_t)method);

      //__ call_VME(CAST_FROM_FN_PTR(address, print_enclave_frame), false, true);

      //__ pop(r11);
    #endif

    if (break_method != NULL &&
        strcmp(method->name()->as_C_string(), break_method) == 0 &&
        strcmp(method->klass_name()->as_C_string(), break_klass) == 0) {
        //__ os_breakpoint();
        //Temporarily comment because it is for debug in macroAssembler
        //break;
    }

    // YYY
//   __ incrementl(rdx);
//   __ andl(rdx, -2);

    // see if we've got enough room on the stack for locals plus overhead.
//    generate_stack_overflow_check();

    // get return address
    // Comment out because it is not in aarch64 templateInterpreter
    //__ pop(rax);

    // r11 changed to r15 because all are caller saved last register
    // is ecall
    if (is_sgx_interface(method) || strncmp(method->name()->as_C_string(), "sgx_hook", 8) == 0) {
        for (int i = 0;i < size_parameters;i++)
        {
            // movptr change to str, times8 change to lsl(3)
            __ str(r15, Address(rlocals, size_parameters - i, Address::lsl(3)));
            __ push(r15);
        }
    }

    // compute beginning of parameters (rlocals)
    __ add(rlocals, esp, r2, ext::uxtx, 3);
    __ sub(rlocals, rlocals, wordSize);

    //Add because it is in hotspot/enclave/cpu/aarch64/vm/templateInterpreter_aarch64.cpp
    // Make room for locals
    __ sub(rscratch1, esp, r3, ext::uxtx, 3);
    __ andr(sp, rscratch1, -16);

    // r3 - # of additional locals
    // allocate space for locals
    // explicitly initialize locals
    for (int i = 0;i < addtional_locals;i++)
    {
        // for r3, hotspot/enclave/cpu/aarch64/vm/templateInterpreter_aarch64.cpp line 1109
        __ str(zr, Address(__ post(rscratch1, wordSize)));// initialize local variables
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

// address TemplateInterpreterGenerator::generate_return_entry_for(TosState state, int step, size_t index_size)
void NormalCompileTask::return_entry(TosState state, int parameter_size) {

    //Change to r2 because reference to use of generate_normal_entry's use of rcx register related to parameter
    Register parameter = r2;

    // Restore stack bottom in case i2c adjusted stack
    __ ldr(esp, Address(rfp, frame::interpreter_frame_last_sp_offset * wordSize));
    // and NULL it as marker that esp is now tos until next java call
    __ str(zr, Address(rfp, frame::interpreter_frame_last_sp_offset * wordSize));
    __ restore_bcp();
    __ restore_locals();
    __ restore_constant_pool_cache();
    __ get_method(rmethod);

//    __ movl(parameter, parameter_size);
    // Restore machine SP
    __ ldr(rscratch1, Address(rmethod, Method::const_offset()));
    __ ldrh(rscratch1, Address(rscratch1, ConstMethod::max_stack_offset()));
    __ add(rscratch1, rscratch1, frame::interpreter_frame_monitor_size() + 2);
    __ ldr(rscratch2,
           Address(rfp, frame::interpreter_frame_initial_sp_offset * wordSize));
    __ sub(rscratch1, rscratch2, rscratch1, ext::uxtw, 3);
    __ andr(sp, rscratch1, -16);

    // the parameters plus the ret address
    __ current_entry->clear_bit_prev(parameter_size + 1);
}

void NormalCompileTask::_return(TosState state) {
    transition(state, state);
    assert(_desc->calls_vm(),
           "inconsistent calls_vm information"); // call in remove_activation

    if (bs->code() == Bytecodes::_return_register_finalizer) {
        assert(state == vtos, "only valid state");

        __ ldr(c_rarg1, aaddress(0));
        __ load_klass(r3, c_rarg1);
        __ ldrw(r3, Address(r3, Klass::access_flags_offset()));
        __ tst(r3, JVM_ACC_HAS_FINALIZER);
        Label skip_register_finalizer;
        __ br(Assembler::EQ, skip_register_finalizer);

        __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::register_finalizer), c_rarg1);

        __ bind(skip_register_finalizer);
    }

    // Issue a StoreStore barrier after all stores but before return
    // from any constructor for any class with a final field.  We don't
    // know if this is a finalizer, so we always do so.
    if (bs->code() == Bytecodes::_return)
        __ membar(MacroAssembler::StoreStore);

    // Narrow result if state is itos but result type is smaller.
    // Need to narrow in the return bytecode rather than in generate_return_entry
    // since compiled code callers expect the result to already be narrowed.
    if (state == itos) {
        __ narrow(r0);
    }

    __ remove_activation(state);
    __ ret(lr);
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
//void NormalCompileTask::locals_index(Register reg, int offset) { Unimplemented(); }

//-----------------------------------------------------------------------------
// Allocation

void NormalCompileTask::_new() {
    transition(vtos, atos);

    __ get_unsigned_2_byte_index_at_bcp(r3, 1);
    Label slow_case;
    Label done;
    Label initialize_header;
    Label initialize_object; // including clearing the fields
    Label allocate_shared;

    __ get_cpool_and_tags(r4, r0);
    // Make sure the class we're about to instantiate has been resolved.
    // This is done before loading InstanceKlass to be consistent with the order
    // how Constant Pool is updated (see ConstantPool::klass_at_put)
    const int tags_offset = Array<u1>::base_offset_in_bytes();
    __ lea(rscratch1, Address(r0, r3, Address::lsl(0)));
    __ lea(rscratch1, Address(rscratch1, tags_offset));
    __ ldarb(rscratch1, rscratch1);
    __ cmp(rscratch1, JVM_CONSTANT_Class);
    __ br(Assembler::NE, slow_case);

    // get InstanceKlass
    __ lea(r4, Address(r4, r3, Address::lsl(3)));
    __ ldr(r4, Address(r4, sizeof(ConstantPool)));

    // make sure klass is initialized & doesn't have finalizer
    // make sure klass is fully initialized
    __ ldrb(rscratch1, Address(r4, InstanceKlass::init_state_offset()));
    __ cmp(rscratch1, InstanceKlass::fully_initialized);
    __ br(Assembler::NE, slow_case);

    // get instance_size in InstanceKlass (scaled to a count of bytes)
    __ ldrw(r3,
            Address(r4,
                    Klass::layout_helper_offset()));
    // test to see if it has a finalizer or is malformed in some way
    __ tbnz(r3, exact_log2(Klass::_lh_instance_slow_path_bit), slow_case);

    // Allocate the instance
    // 1) Try to allocate in the TLAB
    // 2) if fail and the object is large allocate in the shared Eden
    // 3) if the above fails (or is not applicable), go to a slow case
    // (creates a new TLAB, etc.)

    const bool allow_shared_alloc = true;

    // Allocation in the shared Eden, if allowed.
    //
    // r3: instance size in bytes
    if (allow_shared_alloc) {
        __ bind(allocate_shared);

        __ eden_allocate(r0, r3, 0, r10, slow_case);
        __ incr_allocated_bytes(rthread, r3, 0, rscratch1);
    }

    // slow case
    __ bind(slow_case);
    __ get_constant_pool(c_rarg1);
    __ get_unsigned_2_byte_index_at_bcp(c_rarg2, 1);
    __ call_VM(r0, CAST_FROM_FN_PTR(address, InterpreterRuntime::_new), c_rarg1, c_rarg2);
    __ verify_oop(r0);

    // continue
    __ bind(done);
    // Must prevent reordering of stores for object initialization with stores that publish the new object.
    __ membar(Assembler::StoreStore);
}


void NormalCompileTask::newarray() {
    transition(itos, atos);
    __ load_unsigned_byte(c_rarg1, at_bcp(1));
    __ mov(c_rarg2, r0);
    __ call_VM(r0, CAST_FROM_FN_PTR(address, InterpreterRuntime::newarray),
            c_rarg1, c_rarg2);
    // Must prevent reordering of stores for object initialization with stores that publish the new object.
    __ membar(Assembler::StoreStore);
}

void NormalCompileTask::anewarray() {
    transition(itos, atos);
    __ get_unsigned_2_byte_index_at_bcp(c_rarg2, 1);
    __ get_constant_pool(c_rarg1);
    __ mov(c_rarg3, r0);
    __ call_VM(r0, CAST_FROM_FN_PTR(address, InterpreterRuntime::anewarray),
            c_rarg1, c_rarg2, c_rarg3);
    // Must prevent reordering of stores for object initialization with stores that publish the new object.
    __ membar(Assembler::StoreStore);
}

void NormalCompileTask::arraylength() {
    transition(atos, itos);
    __ null_check(r0, arrayOopDesc::length_offset_in_bytes());
    __ ldrw(r0, Address(r0, arrayOopDesc::length_offset_in_bytes()));
}

void NormalCompileTask::multianewarray() {
    transition(vtos, atos);
    __ load_unsigned_byte(r0, at_bcp(3)); // get number of dimensions
    // last dim is on top of stack; we want address of first one:
    // first_addr = last_addr + (ndims - 1) * wordSize
    __ lea(c_rarg1, Address(esp, r0, Address::uxtw(3)));
    __ sub(c_rarg1, c_rarg1, wordSize);
    __ call_VM(r0,
            CAST_FROM_FN_PTR(address, InterpreterRuntime::multianewarray),
            c_rarg1);
    __ load_unsigned_byte(r1, at_bcp(3));
    __ lea(esp, Address(esp, r1, Address::uxtw(3)));
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
    transition(vtos, vtos);

    const Register cache = r2;
    const Register index = r3;
    const Register obj   = r2;
    const Register off   = r19;
    const Register flags = r0;
    const Register bc    = r4;

    resolve_cache_and_index(byte_no, cache, index, sizeof(u2));
    //jvmti_post_field_mod(cache, index, is_static);
    load_field_cp_cache_entry(obj, cache, index, off, flags, is_static);

    Label Done;
    __ mov(r5, flags);

    {
        Label notVolatile;
        __ tbz(r5, ConstantPoolCacheEntry::is_volatile_shift, notVolatile);
        __ membar(MacroAssembler::StoreStore | MacroAssembler::LoadStore);
        __ bind(notVolatile);
    }

    // field address
    const Address field(obj, off);

    Label notByte, notBool, notInt, notShort, notChar,
            notLong, notFloat, notObj, notDouble;

    // x86 uses a shift and mask or wings it with a shift plus assert
    // the mask is not needed. aarch64 just uses bitfield extract
    __ ubfxw(flags, flags, ConstantPoolCacheEntry::tos_state_shift,  ConstantPoolCacheEntry::tos_state_bits);

    assert(btos == 0, "change code, btos != 0");
    __ cbnz(flags, notByte);

    // btos
    {
        __ pop(btos);
        if (!is_static) pop_and_check_object(obj);
        // oopDesc::bs()->interpreter_write_barrier(_masm, obj);
        __ strb(r0, field);
        if (!is_static) {
            patch_bytecode(Bytecodes::_fast_bputfield, bc, r1, true, byte_no);
        }
        __ b(Done);
    }

    __ bind(notByte);
    __ cmp(flags, ztos);
    __ br(Assembler::NE, notBool);

    // ztos
    {
        __ pop(ztos);
        if (!is_static) pop_and_check_object(obj);
        // oopDesc::bs()->interpreter_write_barrier(_masm, obj);
        __ andw(r0, r0, 0x1);
        __ strb(r0, field);
        if (!is_static) {
            patch_bytecode(Bytecodes::_fast_zputfield, bc, r1, true, byte_no);
        }
        __ b(Done);
    }

    __ bind(notBool);
    __ cmp(flags, atos);
    __ br(Assembler::NE, notObj);

    // atos
    {
        __ pop(atos);
        if (!is_static) pop_and_check_object(obj);
        // oopDesc::bs()->interpreter_write_barrier(_masm, obj);
        // Store into the field
        do_oop_store(_masm, field, r0, BarrierSet::Other, false);
        if (!is_static) {
            patch_bytecode(Bytecodes::_fast_aputfield, bc, r1, true, byte_no);
        }
        __ b(Done);
    }

    __ bind(notObj);
    __ cmp(flags, itos);
    __ br(Assembler::NE, notInt);

    // itos
    {
        __ pop(itos);
        if (!is_static) pop_and_check_object(obj);
        // oopDesc::bs()->interpreter_write_barrier(_masm, obj);
        __ strw(r0, field);
        if (!is_static) {
            patch_bytecode(Bytecodes::_fast_iputfield, bc, r1, true, byte_no);
        }
        __ b(Done);
    }

    __ bind(notInt);
    __ cmp(flags, ctos);
    __ br(Assembler::NE, notChar);

    // ctos
    {
        __ pop(ctos);
        if (!is_static) pop_and_check_object(obj);
        // oopDesc::bs()->interpreter_write_barrier(_masm, obj);
        __ strh(r0, field);
        if (!is_static) {
            patch_bytecode(Bytecodes::_fast_cputfield, bc, r1, true, byte_no);
        }
        __ b(Done);
    }

    __ bind(notChar);
    __ cmp(flags, stos);
    __ br(Assembler::NE, notShort);

    // stos
    {
        __ pop(stos);
        if (!is_static) pop_and_check_object(obj);
        // oopDesc::bs()->interpreter_write_barrier(_masm, obj);
        __ strh(r0, field);
        if (!is_static) {
            patch_bytecode(Bytecodes::_fast_sputfield, bc, r1, true, byte_no);
        }
        __ b(Done);
    }

    __ bind(notShort);
    __ cmp(flags, ltos);
    __ br(Assembler::NE, notLong);

    // ltos
    {
        __ pop(ltos);
        if (!is_static) pop_and_check_object(obj);
        // oopDesc::bs()->interpreter_write_barrier(_masm, obj);
        __ str(r0, field);
        if (!is_static) {
            patch_bytecode(Bytecodes::_fast_lputfield, bc, r1, true, byte_no);
        }
        __ b(Done);
    }

    __ bind(notLong);
    __ cmp(flags, ftos);
    __ br(Assembler::NE, notFloat);

    // ftos
    {
        __ pop(ftos);
        if (!is_static) pop_and_check_object(obj);
        // oopDesc::bs()->interpreter_write_barrier(_masm, obj);
        __ strs(v0, field);
        if (!is_static) {
            patch_bytecode(Bytecodes::_fast_fputfield, bc, r1, true, byte_no);
        }
        __ b(Done);
    }

    __ bind(notFloat);
#ifdef ASSERT
    __ cmp(flags, dtos);
  __ br(Assembler::NE, notDouble);
#endif

    // dtos
    {
        __ pop(dtos);
        if (!is_static) pop_and_check_object(obj);
        // oopDesc::bs()->interpreter_write_barrier(_masm, obj);
        __ strd(v0, field);
        if (!is_static) {
            patch_bytecode(Bytecodes::_fast_dputfield, bc, r1, true, byte_no);
        }
    }

#ifdef ASSERT
    __ b(Done);

  __ bind(notDouble);
  __ stop("Bad state");
#endif

    __ bind(Done);

    {
        Label notVolatile;
        __ tbz(r5, ConstantPoolCacheEntry::is_volatile_shift, notVolatile);
        __ membar(MacroAssembler::StoreLoad | MacroAssembler::StoreStore);
        __ bind(notVolatile);
    }
}

void NormalCompileTask::getfield_or_static(int byte_no, bool is_static) {
    const Register cache = r2;
    const Register index = r3;
    const Register obj   = r4;
    const Register off   = r19;
    const Register flags = r0;
    const Register bc    = r4; // uses same reg as obj, so don't mix them

    resolve_cache_and_index(byte_no, cache, index, sizeof(u2));
    jvmti_post_field_access(cache, index, is_static, false);
    load_field_cp_cache_entry(obj, cache, index, off, flags, is_static);

    if (!is_static) {
        // obj is on the stack
        pop_and_check_object(obj);
    }
    // oopDesc::bs()->interpreter_read_barrier_not_null(_masm, obj);

    const Address field(obj, off);

    Label Done, notByte, notBool, notInt, notShort, notChar,
            notLong, notFloat, notObj, notDouble;

    // x86 uses a shift and mask or wings it with a shift plus assert
    // the mask is not needed. aarch64 just uses bitfield extract
    __ ubfxw(flags, flags, ConstantPoolCacheEntry::tos_state_shift,  ConstantPoolCacheEntry::tos_state_bits);

    assert(btos == 0, "change code, btos != 0");
    __ cbnz(flags, notByte);

    // btos
    __ load_signed_byte(r0, field);
    __ push(btos);
    // Rewrite bytecode to be faster
    if (!is_static) {
        //fill with true
        patch_bytecode(Bytecodes::_fast_bgetfield, bc, r1, true, byte_no);
    }
    __ b(Done);

    __ bind(notByte);
    __ cmp(flags, ztos);
    __ br(Assembler::NE, notBool);

    // ztos (same code as btos)
    __ ldrsb(r0, field);
    __ push(ztos);
    // Rewrite bytecode to be faster
    if (!is_static) {
        // use btos rewriting, no truncating to t/f bit is needed for getfield.
        //fill with true
        patch_bytecode(Bytecodes::_fast_bgetfield, bc, r1, true, byte_no);
    }
    __ b(Done);

    __ bind(notBool);
    __ cmp(flags, atos);
    __ br(Assembler::NE, notObj);
    // atos
    __ load_heap_oop(r0, field);
    __ push(atos);
    if (!is_static) {
        //fill with true
        patch_bytecode(Bytecodes::_fast_agetfield, bc, r1, true, byte_no);
    }
    __ b(Done);

    __ bind(notObj);
    __ cmp(flags, itos);
    __ br(Assembler::NE, notInt);
    // itos
    __ ldrw(r0, field);
    __ push(itos);
    // Rewrite bytecode to be faster
    if (!is_static) {
        //fill with true
        patch_bytecode(Bytecodes::_fast_igetfield, bc, r1, true, byte_no);
    }
    __ b(Done);

    __ bind(notInt);
    __ cmp(flags, ctos);
    __ br(Assembler::NE, notChar);
    // ctos
    __ load_unsigned_short(r0, field);
    __ push(ctos);
    // Rewrite bytecode to be faster
    if (!is_static) {
        //fill with true
        patch_bytecode(Bytecodes::_fast_cgetfield, bc, r1, true, byte_no);
    }
    __ b(Done);

    __ bind(notChar);
    __ cmp(flags, stos);
    __ br(Assembler::NE, notShort);
    // stos
    __ load_signed_short(r0, field);
    __ push(stos);
    // Rewrite bytecode to be faster
    if (!is_static) {
        //fill with true
        patch_bytecode(Bytecodes::_fast_sgetfield, bc, r1, true, byte_no);
    }
    __ b(Done);

    __ bind(notShort);
    __ cmp(flags, ltos);
    __ br(Assembler::NE, notLong);
    // ltos
    __ ldr(r0, field);
    __ push(ltos);
    // Rewrite bytecode to be faster
    if (!is_static) {
        //fill with true
        patch_bytecode(Bytecodes::_fast_lgetfield, bc, r1, true, byte_no);
    }
    __ b(Done);

    __ bind(notLong);
    __ cmp(flags, ftos);
    __ br(Assembler::NE, notFloat);
    // ftos
    __ ldrs(v0, field);
    __ push(ftos);
    // Rewrite bytecode to be faster
    if (!is_static) {
        //fill with true
        patch_bytecode(Bytecodes::_fast_fgetfield, bc, r1, true, byte_no);
    }
    __ b(Done);

    __ bind(notFloat);
#ifdef ASSERT
    __ cmp(flags, dtos);
  __ br(Assembler::NE, notDouble);
#endif
    // dtos
    __ ldrd(v0, field);
    __ push(dtos);
    // Rewrite bytecode to be faster
    if (!is_static) {
        //fill with true
        patch_bytecode(Bytecodes::_fast_dgetfield, bc, r1, true, byte_no);
    }
#ifdef ASSERT
        __ b(Done);

  __ bind(notDouble);
  __ stop("Bad state");
#endif

    __ bind(Done);
    // It's really not worth bothering to check whether this field
    // really is volatile in the slow case.
    __ membar(MacroAssembler::LoadLoad | MacroAssembler::LoadStore);

}

// The Rcache and index registers must be set before call
// n.b unlike x86 cache already includes the index offset
void NormalCompileTask::load_field_cp_cache_entry(Register obj,
                                              Register cache,
                                              Register index,
                                              Register off,
                                              Register flags,
                                              bool is_static = false) {
    assert_different_registers(cache, index, flags, off);

    ByteSize cp_base_offset = ConstantPoolCache::base_offset();
    // Field offset
    __ ldr(off, Address(cache, in_bytes(cp_base_offset +
                                        ConstantPoolCacheEntry::f2_offset())));
    // Flags
    __ ldrw(flags, Address(cache, in_bytes(cp_base_offset +
                                           ConstantPoolCacheEntry::flags_offset())));

    // klass overwrite register
    if (is_static) {
        __ ldr(obj, Address(cache, in_bytes(cp_base_offset +
                                            ConstantPoolCacheEntry::f1_offset())));
        const int mirror_offset = in_bytes(Klass::java_mirror_offset());
        __ ldr(obj, Address(obj, mirror_offset));
    }
}

// The registers cache and index expected to be set before call.
// Correct values of the cache and index registers are preserved.
void NormalCompileTask::jvmti_post_field_access(Register cache, Register index,
                                            bool is_static, bool has_tos) {
}

void NormalCompileTask::pop_and_check_object(Register r)
{
    __ pop_ptr(r);
    __ null_check(r);  // for field access must check obj.
    __ verify_oop(r);
}


PatchingStub* NormalCompileTask::resolve_cache_and_index(int byte_no,
                                                         Register Rcache,
                                                         Register index,
                                                         size_t index_size) {
    const Register temp = r19;
    assert_different_registers(Rcache, index, temp);

    Label resolved;
    assert(byte_no == f1_byte || byte_no == f2_byte, "byte_no out of range");
    __ get_cache_and_index_and_bytecode_at_bcp(Rcache, index, temp, byte_no, 1, index_size);
    __ cmp(temp, (int) bs->code());  // have we resolved this bytecode?
    __ br(Assembler::EQ, resolved);

    // resolve first time through
    address entry;
    switch (bs->code()) {
        case Bytecodes::_getstatic:
        case Bytecodes::_putstatic:
        case Bytecodes::_getfield:
        case Bytecodes::_putfield:
            entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_get_put);
            break;
        case Bytecodes::_invokevirtual:
        case Bytecodes::_invokespecial:
        case Bytecodes::_invokestatic:
        case Bytecodes::_invokeinterface:
            entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_invoke);
            break;
        case Bytecodes::_invokehandle:
            entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_invokehandle);
            break;
        case Bytecodes::_invokedynamic:
            entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_invokedynamic);
            break;
        default:
            fatal(err_msg("unexpected bytecode: %s", Bytecodes::name(bs->code())));
            break;
    }
    __ mov(temp, (int) bs->code());
    __ call_VM(noreg, entry, temp);

    // Update registers with resolved info
    __ get_cache_and_index_at_bcp(Rcache, index, 1, index_size);
    // n.b. unlike x86 Rcache is now rcpool plus the indexed offset
    // so all clients ofthis method must be modified accordingly
    __ bind(resolved);

}

void NormalCompileTask::jsr_w() { Unimplemented(); }
void NormalCompileTask::goto_w() {
    branch(false, true);
}

void NormalCompileTask::invokevirtual_helper(Register index,
                                         Register recv,
                                         Register flags)
{
  // Uses temporary registers r0, r3
  assert_different_registers(index, recv, r0, r3);
  // Test for an invoke of a final method
  Label notFinal;
  __ tbz(flags, ConstantPoolCacheEntry::is_vfinal_shift, notFinal);

  const Register method = index;  // method must be rmethod
  assert(method == rmethod,
         "methodOop must be rmethod for interpreter calling convention");

  // do the call - the index is actually the method to call
  // that is, f2 is a vtable index if !is_vfinal, else f2 is a Method*

  // It's final, need a null check here!
  __ null_check(recv);

  // profile this call
  __ profile_final_call(r0);
  __ profile_arguments_type(r0, method, r4, true);

  __ jump_from_interpreted(method, r0);

  __ bind(notFinal);

  // get receiver klass
  __ null_check(recv, oopDesc::klass_offset_in_bytes());
  __ load_klass(r0, recv);

  // profile this call
  __ profile_virtual_call(r0, rlocals, r3);

  // get target methodOop & entry point
  __ lookup_virtual_method(r0, index, method);
  __ profile_arguments_type(r3, method, r4, true);
  // FIXME -- this looks completely redundant. is it?
  // __ ldr(r3, Address(method, Method::interpreter_entry_offset()));
  __ jump_from_interpreted(method, r3);
}

void NormalCompileTask::invokevirtual(int byte_no) {
  transition(vtos, vtos);
  assert(byte_no == f2_byte, "use this argument");

  prepare_invoke(byte_no, rmethod, noreg, r2, r3);

  // rmethod: index (actually a Method*)
  // r2: receiver
  // r3: flags

  invokevirtual_helper(rmethod, r2, r3);
}

void NormalCompileTask::invokespecial(int byte_no) {
    transition(vtos, vtos);
    assert(byte_no == f1_byte, "use this argument");

    prepare_invoke(byte_no, rmethod, noreg,  // get f1 Method*
                   r2, noreg);  // get receiver also for null check
    __ verify_oop(r2);
    __ null_check(r2);
    // do the call
    __ profile_call(r0);
    __ profile_arguments_type(r0, rmethod, rbcp, false);
    __ jump_from_interpreted(rmethod, r0);
}

void NormalCompileTask::invokeinterface(int byte_no) {
    // TODO: fix me
    // transition(vtos, vtos);
    // assert(byte_no == f1_byte, "use this argument");

    // prepare_invoke(byte_no, r0, rmethod,  // get f1 Klass*, f2 Method*
    // 	 r2, r3); // recv, flags

    // // r0: interface klass (from f1)
    // // rmethod: method (from f2)
    // // r2: receiver
    // // r3: flags

    // // Special case of invokeinterface called for virtual method of
    // // java.lang.Object.  See cpCacheOop.cpp for details.
    // // This code isn't produced by javac, but could be produced by
    // // another compliant java compiler.
    // Label notMethod;
    // __ tbz(r3, ConstantPoolCacheEntry::is_forced_virtual_shift, notMethod);

    // invokevirtual_helper(rmethod, r2, r3);
    // __ bind(notMethod);

    // // Get receiver klass into r3 - also a null check
    // __ restore_locals();
    // __ null_check(r2, oopDesc::klass_offset_in_bytes());
    // __ load_klass(r3, r2);

    // Label no_such_interface, no_such_method;

    // // Receiver subtype check against REFC.
    // // Superklass in r0. Subklass in r3. Blows rscratch2, r13.
    // __ lookup_interface_method(// inputs: rec. class, interface, itable index
    //                            r3, r0, noreg,
    //                            // outputs: scan temp. reg, scan temp. reg
    //                            rscratch2, r13,
    //                            no_such_interface,
    //                            /*return_method=*/false);

    // // profile this call
    // __ profile_virtual_call(r3, r13, r19);

    // // Get declaring interface class from method, and itable index
    // __ ldr(r0, Address(rmethod, Method::const_offset()));
    // __ ldr(r0, Address(r0, ConstMethod::constants_offset()));
    // __ ldr(r0, Address(r0, ConstantPool::pool_holder_offset_in_bytes()));
    // __ ldrw(rmethod, Address(rmethod, Method::itable_index_offset()));
    // __ subw(rmethod, rmethod, Method::itable_index_max);
    // __ negw(rmethod, rmethod);

    // __ lookup_interface_method(// inputs: rec. class, interface, itable index
    //                            r3, r0, rmethod,
    //                            // outputs: method, scan temp. reg
    //                            rmethod, r13,
    //                            no_such_interface);

    // // rmethod,: methodOop to call
    // // r2: receiver
    // // Check for abstract method error
    // // Note: This should be done more efficiently via a throw_abstract_method_error
    // //       interpreter entry point and a conditional jump to it in case of a null
    // //       method.
    // __ cbz(rmethod, no_such_method);

    // __ profile_arguments_type(r3, rmethod, r13, true);

    // // do the call
    // // r2: receiver
    // // rmethod,: methodOop
    // __ jump_from_interpreted(rmethod, r3);
    // __ should_not_reach_here();

    // // exception handling code follows...
    // // note: must restore interpreter registers to canonical
    // //       state for exception handling to work correctly!

    // __ bind(no_such_method);
    // // throw exception
    // __ restore_bcp();      // bcp must be correct for exception handler   (was destroyed)
    // __ restore_locals();   // make sure locals pointer is correct as well (was destroyed)
    // __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::throw_AbstractMethodError));
    // // the call_VM checks for exception, so we should never return here.
    // __ should_not_reach_here();

    // __ bind(no_such_interface);
    // // throw exception
    // __ restore_bcp();      // bcp must be correct for exception handler   (was destroyed)
    // __ restore_locals();   // make sure locals pointer is correct as well (was destroyed)
    // __ call_VM(noreg, CAST_FROM_FN_PTR(address,
    //                  InterpreterRuntime::throw_IncompatibleClassChangeError));
    // // the call_VM checks for exception, so we should never return here.
    // __ should_not_reach_here();
    return;

}

void NormalCompileTask::invokestatic(int byte_no) {
  transition(vtos, vtos);
  assert(byte_no == f1_byte, "use this argument");

  prepare_invoke(byte_no, rmethod, noreg, noreg, noreg);  // get f1 Method*
  // do the call
  __ profile_call(r0);
  __ profile_arguments_type(r0, rmethod, r4, false);
  __ jump_from_interpreted(rmethod, r0);
}

void NormalCompileTask::invokedynamic(int byte_no) {
  transition(vtos, vtos);
  assert(byte_no == f1_byte, "use this argument");

  if (!EnableInvokeDynamic) {
    // We should not encounter this bytecode if !EnableInvokeDynamic.
    // The verifier will stop it.  However, if we get past the verifier,
    // this will stop the thread in a reasonable way, without crashing the JVM.
    __ call_VM(noreg, CAST_FROM_FN_PTR(address,
                     InterpreterRuntime::throw_IncompatibleClassChangeError));
    // the call_VM checks for exception, so we should never return here.
    __ should_not_reach_here();
    return;
  }

  prepare_invoke(byte_no, rmethod, r0, noreg, noreg);

  // r0: CallSite object (from cpool->resolved_references[])
  // rmethod: MH.linkToCallSite method (from f2)

  // Note:  r0_callsite is already pushed by prepare_invoke

  // %%% should make a type profile for any invokedynamic that takes a ref argument
  // profile this call
  __ profile_call(rbcp);
  __ profile_arguments_type(r3, rmethod, r13, false);

  __ verify_oop(r0);

  __ jump_from_interpreted(rmethod, r0);
}

void NormalCompileTask::prepare_invoke(int byte_no,
                                   Register method, // linked method (or i-klass)
                                   Register index,  // itable index, MethodType, etc.
                                   Register recv,   // if caller wants to see it
                                   Register flags   // if caller wants to test it
) {
    // determine flags
    Bytecodes::Code code = bs->code();
    const bool is_invokeinterface  = code == Bytecodes::_invokeinterface;
    const bool is_invokedynamic    = code == Bytecodes::_invokedynamic;
    const bool is_invokehandle     = code == Bytecodes::_invokehandle;
    const bool is_invokevirtual    = code == Bytecodes::_invokevirtual;
    const bool is_invokespecial    = code == Bytecodes::_invokespecial;
    const bool load_receiver       = (recv  != noreg);
    const bool save_flags          = (flags != noreg);
    assert(load_receiver == (code != Bytecodes::_invokestatic && code != Bytecodes::_invokedynamic), "");
    assert(save_flags    == (is_invokeinterface || is_invokevirtual), "need flags for vfinal");
    assert(flags == noreg || flags == r3, "");
    assert(recv  == noreg || recv  == r2, "");

    // setup registers & access constant pool cache
    if (recv  == noreg)  recv  = r2;
    if (flags == noreg)  flags = r3;
    assert_different_registers(method, index, recv, flags);

    // save 'interpreter return address'
    __ save_bcp();

    load_invoke_cp_cache_entry(byte_no, method, index, flags, is_invokevirtual, false, is_invokedynamic);

    // maybe push appendix to arguments (just before return address)
    if (is_invokedynamic || is_invokehandle) {
        Label L_no_push;
        __ tbz(flags, ConstantPoolCacheEntry::has_appendix_shift, L_no_push);
        // Push the appendix as a trailing parameter.
        // This must be done before we get the receiver,
        // since the parameter_size includes it.
        __ push(r19);
        __ mov(r19, index);
        assert(ConstantPoolCacheEntry::_indy_resolved_references_appendix_offset == 0, "appendix expected at index+0");
        __ load_resolved_reference_at_index(index, r19);
        __ pop(r19);
        __ push(index);  // push appendix (MethodType, CallSite, etc.)
        __ bind(L_no_push);
    }

    // load receiver if needed (note: no return address pushed yet)
    if (load_receiver) {
        __ andw(recv, flags, ConstantPoolCacheEntry::parameter_size_mask);
        // FIXME -- is this actually correct? looks like it should be 2
        // const int no_return_pc_pushed_yet = -1;  // argument slot correction before we push return address
        // const int receiver_is_at_end      = -1;  // back off one slot to get receiver
        // Address recv_addr = __ argument_address(recv, no_return_pc_pushed_yet + receiver_is_at_end);
        // __ movptr(recv, recv_addr);
        __ add(rscratch1, esp, recv, ext::uxtx, 3); // FIXME: uxtb here?
        __ ldr(recv, Address(rscratch1, -Interpreter::expr_offset_in_bytes(1)));
        __ verify_oop(recv);
    }

    // compute return type
    // x86 uses a shift and mask or wings it with a shift plus assert
    // the mask is not needed. aarch64 just uses bitfield extract
    __ ubfxw(rscratch2, flags, ConstantPoolCacheEntry::tos_state_shift,  ConstantPoolCacheEntry::tos_state_bits);
    // load return address
    {
        const address table_addr = (address) Interpreter::invoke_return_entry_table_for(code);
        __ mov(rscratch1, table_addr);
        __ ldr(lr, Address(rscratch1, rscratch2, Address::lsl(3)));
    }
}

void NormalCompileTask::load_invoke_cp_cache_entry(int byte_no,
                                               Register method,
                                               Register itable_index,
                                               Register flags,
                                               bool is_invokevirtual,
                                               bool is_invokevfinal, /*unused*/
                                               bool is_invokedynamic) {
    // setup registers
    const Register cache = rscratch2;
    const Register index = r4;
    assert_different_registers(method, flags);
    assert_different_registers(method, cache, index);
    assert_different_registers(itable_index, flags);
    assert_different_registers(itable_index, cache, index);
    // determine constant pool cache field offsets
    assert(is_invokevirtual == (byte_no == f2_byte), "is_invokevirtual flag redundant");
    const int method_offset = in_bytes(
            ConstantPoolCache::base_offset() +
            (is_invokevirtual
             ? ConstantPoolCacheEntry::f2_offset()
             : ConstantPoolCacheEntry::f1_offset()));
    const int flags_offset = in_bytes(ConstantPoolCache::base_offset() +
                                      ConstantPoolCacheEntry::flags_offset());
    // access constant pool cache fields
    const int index_offset = in_bytes(ConstantPoolCache::base_offset() +
                                      ConstantPoolCacheEntry::f2_offset());

    size_t index_size = (is_invokedynamic ? sizeof(u4) : sizeof(u2));
    resolve_cache_and_index(byte_no, cache, index, index_size);
    __ ldr(method, Address(cache, method_offset));

    if (itable_index != noreg) {
        __ ldr(itable_index, Address(cache, index_offset));
    }
    __ ldrw(flags, Address(cache, flags_offset));
}

void NormalCompileTask::patch_bytecode(Bytecodes::Code bc, Register bc_reg,
                                   Register temp_reg, bool load_bc_into_bc_reg/*=true*/,
                                   int byte_no)
{
    if (!RewriteBytecodes)  return;
    Label L_patch_done;

    switch (bc) {
        case Bytecodes::_fast_aputfield:
        case Bytecodes::_fast_bputfield:
        case Bytecodes::_fast_zputfield:
        case Bytecodes::_fast_cputfield:
        case Bytecodes::_fast_dputfield:
        case Bytecodes::_fast_fputfield:
        case Bytecodes::_fast_iputfield:
        case Bytecodes::_fast_lputfield:
        case Bytecodes::_fast_sputfield:
        {
            // We skip bytecode quickening for putfield instructions when
            // the put_code written to the constant pool cache is zero.
            // This is required so that every execution of this instruction
            // calls out to InterpreterRuntime::resolve_get_put to do
            // additional, required work.
            assert(byte_no == f1_byte || byte_no == f2_byte, "byte_no out of range");
            assert(load_bc_into_bc_reg, "we use bc_reg as temp");
            __ get_cache_and_index_and_bytecode_at_bcp(temp_reg, bc_reg, temp_reg, byte_no, 1);
            __ movw(bc_reg, bc);
            __ cmpw(temp_reg, (unsigned) 0);
            __ br(Assembler::EQ, L_patch_done);  // don't patch
        }
            break;
        default:
            assert(byte_no == -1, "sanity");
            // the pair bytecodes have already done the load.
            if (load_bc_into_bc_reg) {
                __ movw(bc_reg, bc);
            }
    }


#ifdef ASSERT
    Label L_okay;
  __ load_unsigned_byte(temp_reg, at_bcp(0));
  __ cmpw(temp_reg, (int) Bytecodes::java_code(bc));
  __ br(Assembler::EQ, L_okay);
  __ cmpw(temp_reg, bc_reg);
  __ br(Assembler::EQ, L_okay);
  __ stop("patching the wrong bytecode");
  __ bind(L_okay);
#endif

    // patch bytecode
    __ strb(bc_reg, at_bcp(0));
    __ bind(L_patch_done);
}

// Comment out because not used
/*
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
                    compiled_entry = *callee->enclave_native_function_addr();
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
*/
void NormalCompileTask::checkcast() {
  transition(atos, atos);
  Label done, is_null, ok_is_subtype, quicked, resolved;
  __ cbz(r0, is_null);

  // Get cpool & tags index
  __ get_cpool_and_tags(r2, r3); // r2=cpool, r3=tags array
  __ get_unsigned_2_byte_index_at_bcp(r19, 1); // r19=index
  // See if bytecode has already been quicked
  __ add(rscratch1, r3, Array<u1>::base_offset_in_bytes());
  __ lea(r1, Address(rscratch1, r19));
  __ ldarb(r1, r1);
  __ cmp(r1, JVM_CONSTANT_Class);
  __ br(Assembler::EQ, quicked);

  __ push(atos); // save receiver for result, and for GC
  __ call_VM(r0, CAST_FROM_FN_PTR(address, InterpreterRuntime::quicken_io_cc));
  // vm_result_2 has metadata result
  __ get_vm_result_2(r0, rthread);
  __ pop(r3); // restore receiver
  __ b(resolved);

  // Get superklass in r0 and subklass in r3
  __ bind(quicked);
  __ mov(r3, r0); // Save object in r3; r0 needed for subtype check
  __ lea(r0, Address(r2, r19, Address::lsl(3)));
  __ ldr(r0, Address(r0, sizeof(ConstantPool)));

  __ bind(resolved);
  __ load_klass(r19, r3);

  // Generate subtype check.  Blows r2, r5.  Object in r3.
  // Superklass in r0.  Subklass in r19.
  __ gen_subtype_check(r19, ok_is_subtype);

  // Come here on failure
  __ push(r3);
  // object is at TOS
  __ b(Interpreter::_throw_ClassCastException_entry);

  // Come here on success
  __ bind(ok_is_subtype);
  __ mov(r0, r3); // Restore object in r3

  // Collect counts on whether this test sees NULLs a lot or not.
  if (ProfileInterpreter) {
    __ b(done);
    __ bind(is_null);
    __ profile_null_seen(r2);
  } else {
    __ bind(is_null);   // same as 'done'
  }
  __ bind(done);
}

void NormalCompileTask::instanceof() {
  transition(atos, itos);
  Label done, is_null, ok_is_subtype, quicked, resolved;
  __ cbz(r0, is_null);

  // Get cpool & tags index
  __ get_cpool_and_tags(r2, r3); // r2=cpool, r3=tags array
  __ get_unsigned_2_byte_index_at_bcp(r19, 1); // r19=index
  // See if bytecode has already been quicked
  __ add(rscratch1, r3, Array<u1>::base_offset_in_bytes());
  __ lea(r1, Address(rscratch1, r19));
  __ ldarb(r1, r1);
  __ cmp(r1, JVM_CONSTANT_Class);
  __ br(Assembler::EQ, quicked);

  __ push(atos); // save receiver for result, and for GC
  __ call_VM(r0, CAST_FROM_FN_PTR(address, InterpreterRuntime::quicken_io_cc));
  // vm_result_2 has metadata result
  __ get_vm_result_2(r0, rthread);
  __ pop(r3); // restore receiver
  __ verify_oop(r3);
  __ load_klass(r3, r3);
  __ b(resolved);

  // Get superklass in r0 and subklass in r3
  __ bind(quicked);
  __ load_klass(r3, r0);
  __ lea(r0, Address(r2, r19, Address::lsl(3)));
  __ ldr(r0, Address(r0, sizeof(ConstantPool)));

  __ bind(resolved);

  // Generate subtype check.  Blows r2, r5
  // Superklass in r0.  Subklass in r3.
  __ gen_subtype_check(r3, ok_is_subtype);

  // Come here on failure
  __ mov(r0, 0);
  __ b(done);
  // Come here on success
  __ bind(ok_is_subtype);
  __ mov(r0, 1);

  // Collect counts on whether this test sees NULLs a lot or not.
  if (ProfileInterpreter) {
    __ b(done);
    __ bind(is_null);
    __ profile_null_seen(r2);
  } else {
    __ bind(is_null);   // same as 'done'
  }
  __ bind(done);
  // r0 = 0: obj == NULL or  obj is not an instanceof the specified klass
  // r0 = 1: obj != NULL and obj is     an instanceof the specified klass
}

//need to add to _new, invoke, etc according to /openjdk-sgx/hotspot/enclave_src/share/vm/c0/NormalCompileTask.cpp
void NormalCompileTask::gc_point() {
    // temporarily comment out
    //movptr change to str
    //rbp to fp for bottom of stack
    //__ str(Address(rfp, frame::interpreter_frame_last_sp_offset * wordSize), bs->bci());
    oopSet->put_entry(bs->bci(), __ current_entry->clone());
}

void NormalCompileTask::lookupswitch() {
  transition(itos, itos);
  __ stop("lookupswitch bytecode should have been rewritten");
}

void NormalCompileTask::tableswitch() {
  Label default_case, continue_execution;
  transition(itos, vtos);
  // align rbcp
  __ lea(r1, at_bcp(BytesPerInt));
  __ andr(r1, r1, -BytesPerInt);
  // load lo & hi
  __ ldrw(r2, Address(r1, BytesPerInt));
  __ ldrw(r3, Address(r1, 2 * BytesPerInt));
  __ rev32(r2, r2);
  __ rev32(r3, r3);
  // check against lo & hi
  __ cmpw(r0, r2);
  __ br(Assembler::LT, default_case);
  __ cmpw(r0, r3);
  __ br(Assembler::GT, default_case);
  // lookup dispatch offset
  __ subw(r0, r0, r2);
  __ lea(r3, Address(r1, r0, Address::uxtw(2)));
  __ ldrw(r3, Address(r3, 3 * BytesPerInt));
  __ profile_switch_case(r0, r1, r2);
  // continue execution
  __ bind(continue_execution);
  __ rev32(r3, r3);
  __ load_unsigned_byte(rscratch1, Address(rbcp, r3, Address::sxtw(0)));
  __ add(rbcp, rbcp, r3, ext::sxtw);
  __ dispatch_only(vtos);
  // handle default
  __ bind(default_case);
  __ profile_switch_default(r0);
  __ ldrw(r3, Address(r1, 0));
  __ b(continue_execution);
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
        //if (cc == none) {
        //    __ emit_int8((unsigned char) 0xE9);
        //    __ emit_int32(itr->second - next_addr);
        //} else {
            next_addr += 1;
            __ emit_int8((unsigned char) 0x0F);
            __ emit_int8((unsigned char) (0x80 | cc));
            __ emit_int32(itr->second - next_addr);
        //}
    } else {
        patch_address.push_back(std::pair<int, address>(target, __ pc()));
        //no none condition
        //if (cc == none) {
        //    __ emit_int8((unsigned char) 0xE9);
        //    __ emit_int32(0);
        //} else {
            __ emit_int8((unsigned char) 0x0F);
            __ emit_int8((unsigned char) (0x80 | cc));
            __ emit_int32(0);
        //}
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
    // movptr change to str
    // get sender sp

    // from hotspot/enclave/cpu/aarch64/vm/templateInterpreter_aarch64.cpp
    // line 178, 549
    __ str(esp,
           Address(sp, frame::interpreter_frame_sender_sp_offset * wordSize));
    __ leave();                           // remove frame anchor
    __ pop(ret_addr);                     // get return address
    __ mov(sp, esp);                     // set sp to sender sp
    __ push(ret_addr);
}
/*
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
 */
//not found in templatetable aarch64
void NormalCompileTask::_jmp_return() {
    _return(ret_tos);
    //jmp change to far_jump
    __ far_jump(ret_now);
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
/*
void NormalCompileTask::monitorenter() {
  transition(atos, vtos);

  // check for NULL object
  __ null_check(r0);

  // We need to preemptively evacuate the object, because we later compare
  // it to objects in the BasicObjectLock list, and we might get false negatives
  // if another thread evacuates the object in the meantime. See acmp.
  oopDesc::bs()->interpreter_write_barrier(_masm, r0);

  const Address monitor_block_top(
        rfp, frame::interpreter_frame_monitor_block_top_offset * wordSize);
  const Address monitor_block_bot(
        rfp, frame::interpreter_frame_initial_sp_offset * wordSize);
  const int entry_size = frame::interpreter_frame_monitor_size() * wordSize;

  Label allocated;

  // initialize entry pointer
  __ mov(c_rarg1, zr); // points to free slot or NULL

  // find a free slot in the monitor block (result in c_rarg1)
  {
    Label entry, loop, exit;
    __ ldr(c_rarg3, monitor_block_top); // points to current entry,
                                        // starting with top-most entry
    __ lea(c_rarg2, monitor_block_bot); // points to word before bottom

    __ b(entry);

    __ bind(loop);
    // check if current entry is used
    // if not used then remember entry in c_rarg1
    __ ldr(rscratch1, Address(c_rarg3, BasicObjectLock::obj_offset_in_bytes()));
    __ cmp(zr, rscratch1);
    __ csel(c_rarg1, c_rarg3, c_rarg1, Assembler::EQ);
    // check if current entry is for same object
    __ cmp(r0, rscratch1);
    // if same object then stop searching
    __ br(Assembler::EQ, exit);
    // otherwise advance to next entry
    __ add(c_rarg3, c_rarg3, entry_size);
    __ bind(entry);
    // check if bottom reached
    __ cmp(c_rarg3, c_rarg2);
    // if not at bottom then check this entry
    __ br(Assembler::NE, loop);
    __ bind(exit);
  }

  __ cbnz(c_rarg1, allocated); // check if a slot has been found and
                            // if found, continue with that on

  // allocate one if there's no free slot
  {
    Label entry, loop;
    // 1. compute new pointers            // rsp: old expression stack top
    __ ldr(c_rarg1, monitor_block_bot);   // c_rarg1: old expression stack bottom
    __ sub(esp, esp, entry_size);         // move expression stack top
    __ sub(c_rarg1, c_rarg1, entry_size); // move expression stack bottom
    __ mov(c_rarg3, esp);                 // set start value for copy loop
    __ str(c_rarg1, monitor_block_bot);   // set new monitor block bottom

    __ sub(sp, sp, entry_size);           // make room for the monitor

    __ b(entry);
    // 2. move expression stack contents
    __ bind(loop);
    __ ldr(c_rarg2, Address(c_rarg3, entry_size)); // load expression stack
                                                   // word from old location
    __ str(c_rarg2, Address(c_rarg3, 0));          // and store it at new location
    __ add(c_rarg3, c_rarg3, wordSize);            // advance to next word
    __ bind(entry);
    __ cmp(c_rarg3, c_rarg1);        // check if bottom reached
    __ br(Assembler::NE, loop);      // if not at bottom then
                                     // copy next word
  }

  // call run-time routine
  // c_rarg1: points to monitor entry
  __ bind(allocated);

  // Increment bcp to point to the next bytecode, so exception
  // handling for async. exceptions work correctly.
  // The object has already been poped from the stack, so the
  // expression stack looks correct.
  __ increment(rbcp);

  // store object
  __ str(r0, Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()));
  __ lock_object(c_rarg1);

  // check to make sure this monitor doesn't cause stack overflow after locking
  __ save_bcp();  // in case of exception
  __ generate_stack_overflow_check(0);

  // The bcp has already been incremented. Just need to dispatch to
  // next instruction.
  __ dispatch_next(vtos);
}
*/
/*
void NormalCompileTask::monitorexit() {
  transition(atos, vtos);

  // check for NULL object
  __ null_check(r0);

  // We need to preemptively evacuate the object, because we later compare
  // it to objects in the BasicObjectLock list, and we might get false negatives
  // if another thread evacuates the object in the meantime. See acmp.
  oopDesc::bs()->interpreter_write_barrier(_masm, r0);

  const Address monitor_block_top(
        rfp, frame::interpreter_frame_monitor_block_top_offset * wordSize);
  const Address monitor_block_bot(
        rfp, frame::interpreter_frame_initial_sp_offset * wordSize);
  const int entry_size = frame::interpreter_frame_monitor_size() * wordSize;

  Label found;

  // find matching slot
  {
    Label entry, loop;
    __ ldr(c_rarg1, monitor_block_top); // points to current entry,
                                        // starting with top-most entry
    __ lea(c_rarg2, monitor_block_bot); // points to word before bottom
                                        // of monitor block
    __ b(entry);

    __ bind(loop);
    // check if current entry is for same object
    __ ldr(rscratch1, Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()));
    __ cmp(r0, rscratch1);
    // if same object then stop searching
    __ br(Assembler::EQ, found);
    // otherwise advance to next entry
    __ add(c_rarg1, c_rarg1, entry_size);
    __ bind(entry);
    // check if bottom reached
    __ cmp(c_rarg1, c_rarg2);
    // if not at bottom then check this entry
    __ br(Assembler::NE, loop);
  }

  // error handling. Unlocking was not block-structured
  __ call_VM(noreg, CAST_FROM_FN_PTR(address,
                   InterpreterRuntime::throw_illegal_monitor_state_exception));
  __ should_not_reach_here();

  // call run-time routine
  __ bind(found);
  __ push_ptr(r0); // make sure object is on stack (contract with oopMaps)
  __ unlock_object(c_rarg1);
  __ pop_ptr(r0); // discard object
}
*/
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

//Comment out because it is in x86 format
// fast_compile compile method less than 6 instructions (typically setter and getter)
/*
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
                    Bytecode_field f(methodHandle(method), bs->bci());
                    field_holder_klass = (Klass*)Klass_resolve_get_put(JavaThread::current(), method, f.index(), Bytecodes::java_code(f.code()));
                }
                __ movptr(c_obj, (intptr_t)field_holder_klass);
                __ movptr(c_obj, Address(c_obj, Klass::java_mirror_offset()));
                recv_r = c_obj;
                val_r = tos_reg[0];
            } else {
                recv_r = (Register)tos_reg[0];
                val_r = tos_reg[1];
                tos_count -= 1;
            }

            if (field_entry == NULL || !field_entry->is_resolved(bs->code())) {
                Bytecode_field f(methodHandle(method), bs->bci());
                off = JVM_ENTRY_resolve_get_put_C(JavaThread::current(), method->constants(), f.index(), Bytecodes::java_code(f.code()));
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
*/
#undef __
