//
// Created by jianyu on 11/11/18.
//

#include <precompiled.hpp>
#include <runtime/sharedRuntime.hpp>
#include "c0/bytecodeStream.hpp"
#include "c0/c0_CodeStubs.hpp"
#include "c0/MetadataAccessor.hpp"
#include "c0/c0_NormalCompileTask.hpp"
#include <interpreter/interpreterRuntime.hpp>
#define __ _masm->
#define transition(from, to)

// 1. If we expect vtos, push rax into stacks
// 2. If we expect non-tos, pop to rax
// 3. If we have (vtos, vtos), the tos is decided in the generator

int NormalCompileTask::break_bci = 5;
char* NormalCompileTask::break_method = "<init>";
char* NormalCompileTask::break_klass = "java/lang/ThreadGroup";

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


void NormalCompileTask::do_oop_store(InterpreterMacroAssembler* _masm,
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
    __ str(r6, (Register)(intptr_t)start_heap);
    __ str(r7, (Register)(intptr_t)end_heap);
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
            __ os_breakpoint();
            //Temporarily comment because it is for debug in macroAssembler
        }

        adjust_tos();

        bci_ptr_map.insert(std::pair<int, address>(bs->bci(), __ pc()));
        bci_tos.insert(std::pair<int, TosState>(bs->bci(), tos));

        switch (code) {

            case Bytecodes::_nop:			gen(nop(),		    vtos, vtos);
            case Bytecodes::_aconst_null:	gen(aconst_null(),  vtos, atos);
            case Bytecodes::_iconst_0:    gen(iconst(0),      vtos, itos);
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
            case Bytecodes::_ifne:			gen(if_0cmp(notEqual),	    itos, vtos);
            case Bytecodes::_iflt:			gen(if_0cmp(less),		    itos, vtos);
            case Bytecodes::_ifge:			gen(if_0cmp(greaterEqual),  itos, vtos);
            case Bytecodes::_ifgt:			gen(if_0cmp(greater),		itos, vtos);
            case Bytecodes::_ifle:			gen(if_0cmp(lessEqual),	    itos, vtos);
            case Bytecodes::_if_icmpeq:		gen(if_icmp(equal),		    itos, vtos);
            case Bytecodes::_if_icmpne:		gen(if_icmp(notEqual),		itos, vtos);
            case Bytecodes::_if_icmplt:		gen(if_icmp(less),		    itos, vtos);
            case Bytecodes::_if_icmpge:		gen(if_icmp(greaterEqual),	itos, vtos);
            case Bytecodes::_if_icmpgt:		gen(if_icmp(greater),		itos, vtos);
            case Bytecodes::_if_icmple:		gen(if_icmp(lessEqual),	    itos, vtos);
            case Bytecodes::_if_acmpeq:		gen(if_acmp(equal),		    atos, vtos);
            case Bytecodes::_if_acmpne:		gen(if_acmp(notEqual),		atos, vtos);
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
            //TODO: add aarch64 version for below 2 instructions
            case Bytecodes::_monitorenter:  gen(monitorenter(),     atos, vtos);
            case Bytecodes::_monitorexit:   gen(monitorexit(),      atos, vtos);
            case Bytecodes::_wide:
                Unimplemented();
                break;
            case Bytecodes::_multianewarray:gen(multianewarray(),   vtos, atos);
            case Bytecodes::_ifnull:        gen(if_nullcmp(equal),  atos, vtos);
            case Bytecodes::_ifnonnull:     gen(if_nullcmp(notEqual),  atos, vtos);
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
        __ align(wordSize);
        (*itr)->emit();
    }

//   for (std::deque<CodeStub *>::iterator itr = slow_cases.begin(); itr != slow_cases.end();itr++) {
//       delete (PatchingStub*)(*itr);
//   }

    /*
    if (has_interface) {
      __ bind(no_such_interface);
      //abort
      // movptr change to str
      __ str(r0, Address(r0, -1));
      __ str(r0, Address(r0, 0));
    }
    */

    int offset = __ offset();

    __ align(wordSize);
    __ flush();

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

#undef __
