//
// Created by Maxxie Jiang on 22/11/2018.
//

#include <precompiled.hpp>
#include "NativeCompileTask.hpp"
#include <enclave/sc/EnclaveNative.h>

#define __ _masm->

class NativeParameterIterator {
public:
    int _num_int_args;
    int _num_fp_args;
    bool is_static;
    Symbol *_signature;
    int _offset;
    int _stack_offset;
    C0_MacroAssembler *_masm;
    BasicType _ret;
    NativeParameterIterator(Symbol *signature, C0_MacroAssembler *masm, bool _static) : _signature(signature), _masm(masm), is_static(_static) {
        _num_int_args = (is_static ? 1 : 0);
        _num_fp_args = 0;
        _offset = 0;
        _stack_offset = 0; // don't overwrite return address
    }

    void iterate() {
        SignatureStream ss(_signature);
        int len = 0;
        if (!is_static) {
            pass_object();
        }
        for (;;ss.next()) {
            if (ss.at_return_type()) {
                _ret = ss.type();
                break;
            }

            switch (ss.type()) {
                case T_SHORT:
                case T_BOOLEAN:
                case T_INT:
                case T_BYTE:
                case T_CHAR:    pass_int();     break;
                case T_FLOAT:   pass_float();   break;
                case T_DOUBLE:  pass_double();  break;
                case T_LONG:    pass_long();    break;
                case T_ARRAY:
                case T_OBJECT:  pass_object();  break;
                case T_VOID:                    break;
                default:
                    ShouldNotReachHere();
            }
        }
    }

    void generate_result_handler() {
        address entry = __ pc();
        switch (_ret) {
            case T_BOOLEAN: __ c2bool(rax);            break;
            case T_CHAR   : __ movzwl(rax, rax);       break;
            case T_BYTE   : __ sign_extend_byte(rax);  break;
            case T_SHORT  : __ sign_extend_short(rax); break;
            case T_INT    : /* nothing to do */        break;
            case T_LONG   : /* nothing to do */        break;
            case T_VOID   : /* nothing to do */        break;
            case T_FLOAT  : /* nothing to do */        break;
            case T_DOUBLE : /* nothing to do */        break;
            case T_OBJECT :
            case T_ARRAY:
                // retrieve result from frame
                // __ movptr(rax, Address(rbp, frame::interpreter_frame_oop_temp_offset*wordSize));
                // and verify it
                __ verify_oop(rax);
                break;
            default       : ShouldNotReachHere();
        }
    }

    Register from() { return r14; }
    Register to()   { return rsp; }
    Register temp() { return rscratch1; }
    int offset()      { return _offset; };

    void pass_int() {
        const Address src(from(), Interpreter::local_offset_in_bytes(offset()));
        switch (_num_int_args) {
            case 0:
                __ movl(c_rarg1, src);
                _num_int_args++;
                break;
            case 1:
                __ movl(c_rarg2, src);
                _num_int_args++;
                break;
            case 2:
                __ movl(c_rarg3, src);
                _num_int_args++;
                break;
            case 3:
                __ movl(c_rarg4, src);
                _num_int_args++;
                break;
            case 4:
                __ movl(c_rarg5, src);
                _num_int_args++;
                break;
            default:
                __ movl(rax, src);
                __ movl(Address(to(), _stack_offset), rax);
                _stack_offset += wordSize;
                break;
        }
        _offset += 1;
    }

    void pass_long() {
        const Address src(from(), Interpreter::local_offset_in_bytes(offset() + 1));
        switch (_num_int_args) {
            case 0:
                __ movptr(c_rarg1, src);
                _num_int_args++;
                break;
            case 1:
                __ movptr(c_rarg2, src);
                _num_int_args++;
                break;
            case 2:
                __ movptr(c_rarg3, src);
                _num_int_args++;
                break;
            case 3:
                __ movptr(c_rarg4, src);
                _num_int_args++;
                break;
            case 4:
                __ movptr(c_rarg5, src);
                _num_int_args++;
                break;
            default:
                __ movptr(rax, src);
                __ movptr(Address(to(), _stack_offset), rax);
                _stack_offset += wordSize;
                break;
        }
        _offset += 2;
    }

    void pass_float() {
        const Address src(from(), Interpreter::local_offset_in_bytes(offset()));
        if (_num_fp_args < Argument::n_float_register_parameters_c) {
            __ movflt(as_XMMRegister(_num_fp_args++), src);
        } else {
            __ movl(rax, src);
            __ movl(Address(to(), _stack_offset), rax);
            _stack_offset += wordSize;
        }
        _offset += 1;
    }

    void pass_double() {
        const Address src(from(), Interpreter::local_offset_in_bytes(offset() + 1));
        if (_num_fp_args < Argument::n_float_register_parameters_c) {
            __ movdbl(as_XMMRegister(_num_fp_args++), src);
        } else {
            __ movptr(rax, src);
            __ movptr(Address(to(), _stack_offset), rax);
            _stack_offset += wordSize;
        }
        _offset += 2;
    }

    void pass_object() {
        const Address src(from(), Interpreter::local_offset_in_bytes(offset()));
        switch (_num_int_args) {
            case 0:
                assert(offset() == 0, "argument register 1 can only be (non-null) receiver");
                __ lea(c_rarg1, src);
                _num_int_args++;
                break;
            case 1:
                __ lea(rax, src);
                __ xorl(c_rarg2, c_rarg2);
                __ cmpptr(src, 0);
                __ cmov(Assembler::notEqual, c_rarg2, rax);
                _num_int_args++;
                break;
            case 2:
                __ lea(rax, src);
                __ xorl(c_rarg3, c_rarg3);
                __ cmpptr(src, 0);
                __ cmov(Assembler::notEqual, c_rarg3, rax);
                _num_int_args++;
                break;
            case 3:
                __ lea(rax, src);
                __ xorl(c_rarg4, c_rarg4);
                __ cmpptr(src, 0);
                __ cmov(Assembler::notEqual, c_rarg4, rax);
                _num_int_args++;
                break;
            case 4:
                __ lea(rax, src);
                __ xorl(c_rarg5, c_rarg5);
                __ cmpptr(src, 0);
                __ cmov(Assembler::notEqual, c_rarg5, rax);
                _num_int_args++;
                break;
            default:
                __ lea(rax, src);
                __ xorl(temp(), temp());
                __ cmpptr(src, 0);
                __ cmov(Assembler::notEqual, temp(), rax);
                __ movptr(Address(to(), _stack_offset), temp());
                _stack_offset += wordSize;
                break;
        }
        _offset += 1;
    }
};

void NativeCompileTask::entry() {

    // rbx: Method*
    // r13: sender sp

    method_entry = __ pc();

#ifdef DB_FRAME

  __ push(r11);
  __ mov(c_rarg0, r15_thread);
  __ mov(c_rarg1, r13);
  __ movptr(c_rarg2, (intptr_t)method);

  __ call_VME(CAST_FROM_FN_PTR(address, print_enclave_frame), false, true);

  __ pop(r11);
#endif

    int size_of_parameters = method->size_of_parameters();

    // native calls don't need the stack size check since they have no
    // expression stack and the arguments are already on the stack and
    // we only add a handful of words to the stack

    // rbx: Method*
    // rcx: size of parameters
    // r13: sender sp
    __ pop(rax);                                       // get return address

    // for natives the size of locals is zero

    // compute beginning of parameters (r14)
    __ lea(r14, Address(rsp, size_of_parameters, Address::times_8, -wordSize));

    // add 2 zero-initialized slots for native calls
    // initialize result_handler slot
    __ push((int) NULL_WORD);
    // slot for oop temp
    // (static native method holder mirror/jni oop result)
    __ push((int) NULL_WORD);

    // initialize fixed part of activation frame
    generate_fixed_frame(true);

    // make sure method is native & not abstract

    // Since at this point in the method invocation the exception handler
    // would try to exit the monitor of synchronized methods which hasn't
    // been entered yet, we set the thread local variable
    // _do_not_unlock_if_synchronized to true. The remove_activation will
    // check this flag.

//    const Address do_not_unlock_if_synchronized(r15_thread,
//                                                in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
//    __ movbool(do_not_unlock_if_synchronized, true);

//    Label continue_after_compile;
//    __ bind(continue_after_compile);

//    bang_stack_shadow_pages(true);

    // reset the _do_not_unlock_if_synchronized flag
//    __ movbool(do_not_unlock_if_synchronized, false);

    // check for synchronized methods
    // Must happen AFTER invocation_counter check and stack overflow check,
    // so method is not locked if overflows.
//    if (synchronized) {
//        lock_method();
//    } else {
//         no synchronization necessary
//    }

    // start execution


    // work registers
    Register t = r11;
    int log_stack_parameters = size_of_parameters << Interpreter::logStackElementSize;

    __ subptr(rsp, log_stack_parameters);
    __ subptr(rsp, frame::arg_reg_save_area_bytes); // windows
    __ andptr(rsp, -16); // must be 16 byte boundary (see amd64 ABI)

    NativeParameterIterator signature_handler(method->signature(), _masm, method->is_static());

    signature_handler.iterate();

    if (method->is_static()) {
        __ movptr(Address(rbp, frame::interpreter_frame_oop_temp_offset * wordSize),
                  (intptr_t)method->constants()->pool_holder()->java_mirror());
        __ lea(c_rarg1,
               Address(rbp, frame::interpreter_frame_oop_temp_offset * wordSize));
    }

    address native_entry = (address)EnclaveNative::resolve_function(method);
    if (native_entry == NULL) {
        // handle unsatisfied error
    }

    // pass JNIEnv
    __ lea(c_rarg0, Address(r15_thread, JavaThread::jni_environment_offset()));

    // It is enough that the pc() points into the right code
    // segment. It does not have to be the correct return pc.
    __ set_last_Java_frame(rsp, rbp, (address) __ pc());

    // change thread state

    // Change state to native

//  __ movl(Address(r15_thread, JavaThread::thread_state_offset()),
//          _thread_in_native);

//#ifdef ENCLAVE_UNIX
    // TODO: if the code is ready, then there are no exceptions
    __ movptr(Address(r15_thread, Thread::pending_exception_offset()), (int32_t) NULL_WORD);
//#endif

    // Call the native method.
    __ call(RuntimeAddress(native_entry));
    // result potentially in rax or xmm0

    // Verify or restore cpu control state after JNI call
    __ restore_cpu_control_state_after_jni();

    // NOTE: The order of these pushes is known to frame::interpreter_frame_result
    // in order to extract the result of a method call. If the order of these
    // pushes change or anything else is added to the stack then the code in
    // interpreter_frame_result must also change.

    __ push(dtos);
    __ push(ltos);

    // change thread state
//  __ movl(Address(r15_thread, JavaThread::thread_state_offset()),
//          _thread_in_native_trans);

    if (os::is_MP()) {
        if (UseMembar) {
            // Force this write out before the read below
            __ membar(Assembler::Membar_mask_bits(
                    Assembler::LoadLoad | Assembler::LoadStore |
                    Assembler::StoreLoad | Assembler::StoreStore));
        } else {
            // Write serialization page so VM thread can do a pseudo remote membar.
            // We use the current thread pointer to calculate a thread specific
            // offset to write to within the page. This minimizes bus traffic
            // due to cache line collision.
            __ serialize_memory(r15_thread, rscratch2);
        }
    }

    // check for safepoint operation in progress and/or pending suspend requests
    {
        //TODO: safe point?
//    Label Continue;
//    __ cmp32(ExternalAddress(SafepointSynchronize::address_of_state()),
//             SafepointSynchronize::_not_synchronized);
//
//    Label L;
//    __ jcc(Assembler::notEqual, L);
//    __ cmpl(Address(r15_thread, JavaThread::suspend_flags_offset()), 0);
//    __ jcc(Assembler::equal, Continue);
//    __ bind(L);
//
//    // Don't use call_VM as it will see a possible pending exception
//    // and forward it and never return here preventing us from
//    // clearing _last_native_pc down below.  Also can't use
//    // call_VM_leaf either as it will check to see if r13 & r14 are
//    // preserved and correspond to the bcp/locals pointers. So we do a
//    // runtime call by hand.
//    //
//    __ mov(c_rarg0, r15_thread);
//    __ mov(r12, rsp); // remember sp (can only use r12 if not using call_VM)
//    __ subptr(rsp, frame::arg_reg_save_area_bytes); // windows
//    __ andptr(rsp, -16); // align stack as required by ABI
//    __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, JavaThread::check_special_condition_for_native_trans)));
//    __ mov(rsp, r12); // restore sp
//    __ reinit_heapbase();
//    __ bind(Continue);
    }

    // change thread state
//  __ movl(Address(r15_thread, JavaThread::thread_state_offset()), _thread_in_Java);

    // reset_last_Java_frame
    __ reset_last_Java_frame(r15_thread, true);

    // reset handle block
//  __ movptr(t, Address(r15_thread, JavaThread::active_handles_offset()));
//  __ movl(Address(t, JNIHandleBlock::top_offset_in_bytes()), (int32_t)NULL_WORD);

    // If result is an oop unbox and store it in frame where gc will see it
    // and result handler will pick it up

    // TODO: jianyu - we do not box result from native, but will gc ignore it ?
/*    {
        Label no_oop, store_result;
        __ lea(t, ExternalAddress(AbstractInterpreter::result_handler(T_OBJECT)));
        __ cmpptr(t, Address(rbp, frame::interpreter_frame_result_handler_offset*wordSize));
        __ jcc(Assembler::notEqual, no_oop);
        // retrieve result
        __ pop(ltos);
        __ testptr(rax, rax);
        __ jcc(Assembler::zero, store_result);

        __ bind(store_result);
        __ movptr(Address(rbp, frame::interpreter_frame_oop_temp_offset*wordSize), rax);
        // keep stack depth as expected by pushing oop which will eventually be discarde
        __ push(ltos);
        __ bind(no_oop);
    }*/


    {
        //TODO-jianyu: remove the guard
//    Label no_reguard;
//    __ cmpl(Address(r15_thread, JavaThread::stack_guard_state_offset()),
//            JavaThread::stack_guard_yellow_disabled);
//    __ jcc(Assembler::notEqual, no_reguard);
//
//    __ pusha(); // XXX only save smashed registers
//    __ mov(r12, rsp); // remember sp (can only use r12 if not using call_VM)
//    __ subptr(rsp, frame::arg_reg_save_area_bytes); // windows
//    __ andptr(rsp, -16); // align stack as required by ABI
//    __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, SharedRuntime::reguard_yellow_pages)));
//    __ mov(rsp, r12); // restore sp
//    __ popa(); // XXX only restore smashed registers
//    __ reinit_heapbase();

//    __ bind(no_reguard);
    }


    // The method register is junk from after the thread_in_native transition
    // until here.  Also can't call_VM until the bcp has been
    // restored.  Need bcp for throwing exception below so get it now.

    // restore r13 to have legal interpreter frame, i.e., bci == 0 <=>
    // r13 == code_base()

//    __ movptr(r13, (intptr_t)method->code_base());    // get codebase
    // handle exceptions (exception handling will handle unlocking!)
//    {
//        Label L;
//        __ cmpptr(Address(r15_thread, Thread::pending_exception_offset()), (int32_t) NULL_WORD);
//        __ jcc(Assembler::zero, L);
//        // Note: At some point we may want to unify this with the code
//        // used in call_VM_base(); i.e., we should use the
//        // StubRoutines::forward_exception code. For now this doesn't work
//        // here because the rsp is not correctly set at this point.
//        __ MacroAssembler::call_VM(noreg,
//                                   CAST_FROM_FN_PTR(address,
//                                                    InterpreterRuntime::throw_pending_exception));
//        __ should_not_reach_here();
//        __ bind(L);
//    }

    // do unlocking if necessary
//    {
//        Label L;
//        __ movl(t, Address(method, Method::access_flags_offset()));
//        __ testl(t, JVM_ACC_SYNCHRONIZED);
//        __ jcc(Assembler::zero, L);
//        // the code below should be shared with interpreter macro
//        // assembler implementation
//        {
//            Label unlock;
//            // BasicObjectLock will be first in list, since this is a
//            // synchronized method. However, need to check that the object
//            // has not been unlocked by an explicit monitorexit bytecode.
//            const Address monitor(rbp,
//                                  (intptr_t)(frame::interpreter_frame_initial_sp_offset *
//                                             wordSize - sizeof(BasicObjectLock)));
//
//            // monitor expect in c_rarg1 for slow unlock path
//            __ lea(c_rarg1, monitor); // address of first monitor
//
//            __ movptr(t, Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()));
//            __ testptr(t, t);
//            __ jcc(Assembler::notZero, unlock);
//
//            // Entry already unlocked, need to throw exception
//            __ MacroAssembler::call_VM(noreg,
//                                       CAST_FROM_FN_PTR(address,
//                                                        InterpreterRuntime::throw_illegal_monitor_state_exception));
//            __ should_not_reach_here();
//
//            __ bind(unlock);
//            __ unlock_object(c_rarg1);
//        }
//        __ bind(L);
//    }

    // restore potential result in edx:eax, call result handler to
    // restore potential result in ST0 & handle result

    __ pop(ltos);
    __ pop(dtos);

    signature_handler.generate_result_handler();

    #ifdef DB_FRAME
      __ pusha();
      __ movptr(c_rarg1, (intptr_t)method);
      __ call_VME(CAST_FROM_FN_PTR(address, exit_enclave_frame), false, true);
      __ popa();
    #endif
    // remove activation
    __ movptr(t, Address(rbp,
                         frame::interpreter_frame_sender_sp_offset *
                         wordSize)); // get sender sp
    __ leave();                                // remove frame anchor
    __ pop(rdi);                               // get return address
    __ mov(rsp, t);                            // set sp to sender sp
    __ push(rdi);
    __ ret(0);
}

int NativeCompileTask::compile(int size) {
    BufferBlob* stub_blob = BufferBlob::create("EnclaveABI:", 4096);
    if (stub_blob == NULL) {
        ShouldNotReachHere();
    }
    CodeBuffer c(stub_blob);
    _masm = new C0_MacroAssembler(&c, NULL);

    entry();
}

#undef __
