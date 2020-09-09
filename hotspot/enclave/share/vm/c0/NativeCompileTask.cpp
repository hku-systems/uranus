//
// Created by Maxxie Jiang on 22/11/2018.
//

#include <precompiled.hpp>
#include "NativeCompileTask.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include <enclave/sc/EnclaveNative.h>

#define __ _masm->

class NativeParameterIterator: public NativeSignatureIterator {
private:
    MacroAssembler* _masm;
    unsigned int _call_format;
    unsigned int _num_fp_args;
    unsigned int _num_int_args;
    int _stack_offset;
    BasicType return_type;


    void pass_int();
    void pass_long();
    void pass_float();
    void pass_double();
    void pass_object();

public:
    // Creation
    NativeParameterIterator(methodHandle method, MacroAssembler *masm) : NativeSignatureIterator(method) {
      _masm = masm;
      _num_int_args = (method->is_static() ? 1 : 0);
      _num_fp_args = 0;
      _stack_offset = 0;
      return_type = method->result_type();
    }

    // Code generation
    void generate(uint64_t fingerprint) {
      iterate(fingerprint);
    }

    // Code generation support
    static Register from();
    static Register to();
    static Register temp();

    void generate_result_handler();
};

Register NativeParameterIterator::from() { return rlocals; }
Register NativeParameterIterator::to()   { return sp; }
Register NativeParameterIterator::temp() { return rscratch1; }

void NativeParameterIterator::pass_int() {
  const Address src(from(), Interpreter::local_offset_in_bytes(offset()));

  switch (_num_int_args) {
    case 0:
      __ ldr(c_rarg1, src);
      _num_int_args++;
      break;
    case 1:
      __ ldr(c_rarg2, src);
      _num_int_args++;
      break;
    case 2:
      __ ldr(c_rarg3, src);
      _num_int_args++;
      break;
    case 3:
      __ ldr(c_rarg4, src);
      _num_int_args++;
      break;
    case 4:
      __ ldr(c_rarg5, src);
      _num_int_args++;
      break;
    case 5:
      __ ldr(c_rarg6, src);
      _num_int_args++;
      break;
    case 6:
      __ ldr(c_rarg7, src);
      _num_int_args++;
      break;
    default:
      __ ldr(r0, src);
      __ str(r0, Address(to(), _stack_offset));
      _stack_offset += wordSize;
      _num_int_args++;
      break;
  }
}

void NativeParameterIterator::pass_long() {
  const Address src(from(), Interpreter::local_offset_in_bytes(offset() + 1));

  switch (_num_int_args) {
    case 0:
      __ ldr(c_rarg1, src);
      _num_int_args++;
      break;
    case 1:
      __ ldr(c_rarg2, src);
      _num_int_args++;
      break;
    case 2:
      __ ldr(c_rarg3, src);
      _num_int_args++;
      break;
    case 3:
      __ ldr(c_rarg4, src);
      _num_int_args++;
      break;
    case 4:
      __ ldr(c_rarg5, src);
      _num_int_args++;
      break;
    case 5:
      __ ldr(c_rarg6, src);
      _num_int_args++;
      break;
    case 6:
      __ ldr(c_rarg7, src);
      _num_int_args++;
      break;
    default:
      __ ldr(r0, src);
      __ str(r0, Address(to(), _stack_offset));
      _stack_offset += wordSize;
      _num_int_args++;
      break;
  }
}

void NativeParameterIterator::pass_float() {
  const Address src(from(), Interpreter::local_offset_in_bytes(offset()));

  if (_num_fp_args < Argument::n_float_register_parameters_c) {
    __ ldrs(as_FloatRegister(_num_fp_args++), src);
  } else {
    __ ldrw(r0, src);
    __ strw(r0, Address(to(), _stack_offset));
    _stack_offset += wordSize;
    _num_fp_args++;
  }
}

void NativeParameterIterator::pass_double() {
  const Address src(from(), Interpreter::local_offset_in_bytes(offset() + 1));

  if (_num_fp_args < Argument::n_float_register_parameters_c) {
    __ ldrd(as_FloatRegister(_num_fp_args++), src);
  } else {
    __ ldr(r0, src);
    __ str(r0, Address(to(), _stack_offset));
    _stack_offset += wordSize;
    _num_fp_args++;
  }
}

void NativeParameterIterator::pass_object() {

  switch (_num_int_args) {
    case 0:
      assert(offset() == 0, "argument register 1 can only be (non-null) receiver");
      __ add(c_rarg1, from(), Interpreter::local_offset_in_bytes(offset()));
      _num_int_args++;
      break;
    case 1:
    {
      __ add(r0, from(), Interpreter::local_offset_in_bytes(offset()));
      __ mov(c_rarg2, 0);
      __ ldr(temp(), r0);
      Label L;
      __ cbz(temp(), L);
      __ mov(c_rarg2, r0);
      __ bind(L);
      _num_int_args++;
      break;
    }
    case 2:
    {
      __ add(r0, from(), Interpreter::local_offset_in_bytes(offset()));
      __ mov(c_rarg3, 0);
      __ ldr(temp(), r0);
      Label L;
      __ cbz(temp(), L);
      __ mov(c_rarg3, r0);
      __ bind(L);
      _num_int_args++;
      break;
    }
    case 3:
    {
      __ add(r0, from(), Interpreter::local_offset_in_bytes(offset()));
      __ mov(c_rarg4, 0);
      __ ldr(temp(), r0);
      Label L;
      __ cbz(temp(), L);
      __ mov(c_rarg4, r0);
      __ bind(L);
      _num_int_args++;
      break;
    }
    case 4:
    {
      __ add(r0, from(), Interpreter::local_offset_in_bytes(offset()));
      __ mov(c_rarg5, 0);
      __ ldr(temp(), r0);
      Label L;
      __ cbz(temp(), L);
      __ mov(c_rarg5, r0);
      __ bind(L);
      _num_int_args++;
      break;
    }
    case 5:
    {
      __ add(r0, from(), Interpreter::local_offset_in_bytes(offset()));
      __ mov(c_rarg6, 0);
      __ ldr(temp(), r0);
      Label L;
      __ cbz(temp(), L);
      __ mov(c_rarg6, r0);
      __ bind(L);
      _num_int_args++;
      break;
    }
    case 6:
    {
      __ add(r0, from(), Interpreter::local_offset_in_bytes(offset()));
      __ mov(c_rarg7, 0);
      __ ldr(temp(), r0);
      Label L;
      __ cbz(temp(), L);
      __ mov(c_rarg7, r0);
      __ bind(L);
      _num_int_args++;
      break;
    }
    default:
    {
      __ add(r0, from(), Interpreter::local_offset_in_bytes(offset()));
      __ ldr(temp(), r0);
      Label L;
      __ cbnz(temp(), L);
      __ mov(r0, zr);
      __ bind(L);
      __ str(r0, Address(to(), _stack_offset));
      _stack_offset += wordSize;
      _num_int_args++;
      break;
    }
  }
}

void NativeParameterIterator::generate_result_handler() {
  BasicType type = return_type;
  address entry = __ pc();
  switch (type) {
    case T_BOOLEAN: __ c2bool(r0);          break;
    case T_CHAR   : __ uxth(r0, r0);        break;
    case T_BYTE   : __ sxtb(r0, r0);        break;
    case T_SHORT  : __ sxth(r0, r0);        break;
    case T_INT    : __ uxtw(r0, r0);        break;  // FIXME: We almost certainly don't need this
    case T_LONG   : /* nothing to do */        break;
    case T_VOID   : /* nothing to do */        break;
    case T_FLOAT  : /* nothing to do */        break;
    case T_DOUBLE : /* nothing to do */        break;
    case T_OBJECT :
//      // retrieve result from frame
//      __ ldr(r0, Address(rfp, frame::interpreter_frame_oop_temp_offset*wordSize));
      // and verify it
      __ verify_oop(r0);
      break;
    default       : ShouldNotReachHere();
  }
}

void NativeCompileTask::entry() {
    // r1: Method*
    // rscratch1: sender sp

    method_entry = __ pc();

    const int size_of_parameters = method->size_of_parameters();

    // native calls don't need the stack size check since they have no
    // expression stack and the arguments are already on the stack and
    // we only add a handful of words to the stack

    // rmethod: Method*
    // r2: size of parameters
    // rscratch1: sender sp

    // for natives the size of locals is zero

    // compute beginning of parameters (rlocals)
    __ add(rlocals, esp, (size_of_parameters - 1) * wordSize);

    // Pull SP back to minimum size: this avoids holes in the stack
    __ andr(sp, esp, -16);

    // initialize fixed part of activation frame
    generate_fixed_frame(true);

    // Since at this point in the method invocation the exception
    // handler would try to exit the monitor of synchronized methods
    // which hasn't been entered yet, we set the thread local variable
    // _do_not_unlock_if_synchronized to true. The remove_activation
    // will check this flag.
    // TODO: do_not_lock_if_synchronized

    // check for synchronized methods
    // Must happen AFTER invocation_counter check and stack overflow check,
    // so method is not locked if overflows.
    if (method->is_synchronized()) {
//        __ lock_method();
    }

    // work registers
    const Register t = r17;

    __ sub(rscratch1, esp, size_of_parameters * wordSize);
    __ andr(sp, rscratch1, -16);
    __ mov(esp, rscratch1);

    // push parameters
    uint64_t fingerprint = Fingerprinter(method).fingerprint();
    NativeParameterIterator native_itr(methodHandle(method), _masm);
    native_itr.generate(fingerprint);

    // pass mirror handle if static call
    {
        if (method->is_static()) {
            // get mirror
            __ movptr(t, (intptr_t)method->method_holder()->java_mirror());
            // copy mirror into activation frame
            __ str(t, Address(rfp, frame::interpreter_frame_oop_temp_offset * wordSize));
            // pass handle to mirror
            __ add(c_rarg1, rfp, frame::interpreter_frame_oop_temp_offset * wordSize);
        }
    }

    // get native function entry point in r10
    __ ldr(r10, Address(rmethod, Method::native_function_offset()));

    // pass JNIEnv
    __ add(c_rarg0, rthread, in_bytes(JavaThread::jni_environment_offset()));

    // It is enough that the pc() points into the right code
    // segment. It does not have to be the correct return pc.
    __ set_last_Java_frame(esp, rfp, (address)NULL, rscratch1);

    // Change state to native
    __ mov(rscratch1, _thread_in_native);
    __ lea(rscratch2, Address(rthread, JavaThread::thread_state_offset()));
    __ stlrw(rscratch1, rscratch2);

    EnclaveNative::resolve_function(method);
    // Call the native method.
    __ blr(r10);
    __ maybe_isb();
    __ get_method(rmethod);
    // result potentially in r0 or v0

    // change thread state
    __ mov(rscratch1, _thread_in_native_trans);
    __ lea(rscratch2, Address(rthread, JavaThread::thread_state_offset()));
    __ stlrw(rscratch1, rscratch2);

    if (os::is_MP()) {
        if (UseMembar) {
            // Force this write out before the read below
            __ dsb(Assembler::SY);
        } else {
            // Write serialization page so VM thread can do a pseudo remote membar.
            // We use the current thread pointer to calculate a thread specific
            // offset to write to within the page. This minimizes bus traffic
            // due to cache line collision.
            __ serialize_memory(rthread, rscratch2);
        }
    }

    // check for safepoint operation in progress and/or pending suspend requests
    {

    }

    // change thread state
    __ mov(rscratch1, _thread_in_Java);
    __ lea(rscratch2, Address(rthread, JavaThread::thread_state_offset()));
    __ stlrw(rscratch1, rscratch2);

    // reset_last_Java_frame
    __ reset_last_Java_frame(true);

    // If result is an oop unbox and store it in frame where gc will see it
    // and result handler will pick it up
    if (method->is_returning_oop()) {
        // Unbox oop result, e.g. JNIHandles::resolve result.
//        __ ldr(r0, Address(r0, 0));
    }


    // restore bcp to have legal interpreter frame, i.e., bci == 0 <=>
    // rbcp == code_base()
    __ ldr(rbcp, Address(rmethod, Method::const_offset()));   // get ConstMethod*
    __ add(rbcp, rbcp, in_bytes(ConstMethod::codes_offset()));          // get codebase
    // handle exceptions (exception handling will handle unlocking!)
    {
        Label L;
        __ ldr(rscratch1, Address(rthread, Thread::pending_exception_offset()));
        __ cbz(rscratch1, L);
        // Note: At some point we may want to unify this with the code
        // used in call_VM_base(); i.e., we should use the
        // StubRoutines::forward_exception code. For now this doesn't work
        // here because the rsp is not correctly set at this point.
        __ MacroAssembler::call_VM(noreg,
                                   CAST_FROM_FN_PTR(address,
                                                    InterpreterRuntime::throw_pending_exception));
        __ should_not_reach_here();
        __ bind(L);
    }

    // do unlocking if necessary
    {
        if (method->is_synchronized()) {
            Label unlock;
            // BasicObjectLock will be first in list, since this is a
            // synchronized method. However, need to check that the object
            // has not been unlocked by an explicit monitorexit bytecode.

            // monitor expect in c_rarg1 for slow unlock path
            __ lea (c_rarg1, Address(rfp,   // address of first monitor
                                     (long int)(frame::interpreter_frame_initial_sp_offset *
                                                wordSize - sizeof(BasicObjectLock))));

            __ ldr(t, Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()));
            __ cbnz(t, unlock);

            // Entry already unlocked, need to throw exception
            __ MacroAssembler::call_VM(noreg,
                                       CAST_FROM_FN_PTR(address,
                                                        InterpreterRuntime::throw_illegal_monitor_state_exception));
            __ should_not_reach_here();

            __ bind(unlock);
            __ unlock_object(c_rarg1);
        }
    }

    native_itr.generate_result_handler();

    // remove activation
    __ ldr(esp, Address(rfp,
                        frame::interpreter_frame_sender_sp_offset *
                        wordSize)); // get sender sp
    // remove frame anchor
    __ leave();

    // resture sender sp
    __ mov(sp, esp);

    __ ret(lr);

}

int NativeCompileTask::compile(int size) {
    BufferBlob* stub_blob = BufferBlob::create("EnclaveABI:", 4096);
    if (stub_blob == NULL) {
        ShouldNotReachHere();
    }
    CodeBuffer c(stub_blob);
    _masm = new C0_MacroAssembler(&c, NULL);

    entry();

    __ flush();
}

#undef __
