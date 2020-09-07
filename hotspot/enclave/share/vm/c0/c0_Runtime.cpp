//
// Created by jianyu on 11/15/18.
//

#include "StubAssembler.hpp"
#include "precompiled.hpp"
#include <interpreter/interpreterRuntime.hpp>
#include <classfile/symbolTable.hpp>
#include "c0_Runtime.hpp"
#include "c0_CodeStubs.hpp"
#include "Disassembler.hpp"

#include "MetadataAccessor.hpp"
#define __ sasm->

enum {
    max_array_allocation_length = 0x01000000 // sparc friendly value, requires sethi only
};

static void save_live_registers(StubAssembler* sasm,
                                bool save_fpu_registers = true) {
    __ block_comment("save_live_registers");

    __ push(RegSet::range(r0, r29), sp);         // integer registers except lr & sp

    if (save_fpu_registers) {
        for (int i = 30; i >= 0; i -= 2)
            __ stpd(as_FloatRegister(i), as_FloatRegister(i+1),
                    Address(__ pre(sp, -2 * wordSize)));
    } else {
        __ add(sp, sp, -32 * wordSize);
    }

}

static void restore_fpu(StubAssembler* sasm, bool restore_fpu_registers = true) {

}

static void restore_live_registers(StubAssembler* sasm, bool restore_fpu_registers = true) {
    if (restore_fpu_registers) {
        for (int i = 0; i < 32; i += 2)
            __ ldpd(as_FloatRegister(i), as_FloatRegister(i+1),
                    Address(__ post(sp, 2 * wordSize)));
    } else {
        __ add(sp, sp, 32 * wordSize);
    }

    __ pop(RegSet::range(r0, r29), sp);
}

static void restore_live_registers_except_r0(StubAssembler* sasm, bool restore_fpu_registers = true) {

    if (restore_fpu_registers) {
        for (int i = 0; i < 32; i += 2)
            __ ldpd(as_FloatRegister(i), as_FloatRegister(i+1),
                    Address(__ post(sp, 2 * wordSize)));
    } else {
        __ add(sp, sp, 32 * wordSize);
    }

    __ ldp(zr, r1, Address(__ post(sp, 16)));
    __ pop(RegSet::range(r2, r29), sp);
}

Klass* resolve_field_return_klass(methodHandle caller, int bci, TRAPS) {
    Bytecode_field field_access(caller, bci);
    // This can be static or non-static field access
    Bytecodes::Code code       = field_access.code();

    // We must load class, initialize class and resolvethe field
    fieldDescriptor result; // initialize class if needed
    constantPoolHandle constants(THREAD, caller->constants());
    LinkResolver::resolve_field_access(result, constants, field_access.index(), Bytecodes::java_code(code), CHECK_NULL);
    return result.field_holder();
}

class StubFrame: public StackObj {
private:
    StubAssembler* _sasm;

public:
    StubFrame(StubAssembler* sasm, const char* name, bool must_gc_arguments) {
       _sasm = sasm;
       _sasm->enter();
    }
    void load_argument(int offset_in_words, Register reg) {}

    ~StubFrame() {
       _sasm->leave();
       _sasm->ret(lr);
    }
};

CodeBlob* Runtime0::_blobs[Runtime0::number_of_ids];
const char *Runtime0::_blob_names[] = {
        RUNTIME1_STUBS(STUB_NAME, LAST_STUB_NAME)
};

void Runtime0::generate_blob_for(BufferBlob *buffer_blob, Runtime0::StubID id) {
    ResourceMark rm;
    // create code buffer for code storage
    CodeBuffer code(buffer_blob);

    JCompiler::setup_code_buffer(&code, 0);

    // create assembler for code generation
    StubAssembler* sasm = new StubAssembler(&code, name_for(id), id);
    // generate code for runtime stub
    OopMapSet* oop_maps;
    generate_code_for(id, sasm);
    assert(oop_maps == NULL || sasm->frame_size() != no_frame_size,
           "if stub has an oop map it must have a valid frame size");

    // align so printing shows nop's instead of random code at the end (SimpleStubs are aligned)
    sasm->align(BytesPerWord);
    // make sure all code is in code buffer
    sasm->flush();
    // create blob - distinguish a few special cases
    CodeBlob* blob = RuntimeStub::new_runtime_stub(name_for(id),
                                                   &code,
                                                   CodeOffsets::frame_never_safe,
                                                   sasm->frame_size(),
                                                   oop_maps,
                                                   sasm->must_gc_arguments());
    // install blob
    assert(blob != NULL, "blob must exist");
    _blobs[id] = blob;
}

void Runtime0::generate_code_for(Runtime0::StubID id, StubAssembler *sasm) {
    // for better readability
    const bool must_gc_arguments = true;
    const bool dont_gc_arguments = false;

    // default value; overwritten for some optimized stubs that are called from methods that do not use the fpu
    bool save_fpu_registers = true;

    // stub code & info for the different stubs
    OopMapSet* oop_maps = NULL;
    switch (id) {


        case new_instance_id: {
            Register klass = r3;
            Register obj = r0;
            Register obj_size = r2;
            Register t1 = r19;
            Register t2 = r4;

            // * register mapping:
            // * rdx => r3
            // * rax => r0
            // * rcx => r2
            // * rbx => r19
            // * rsi => r4

            __ stp(r5, r19, Address(__ pre(sp, -2 * wordSize)));

            // * do not know below two instruction's use
            //__ push(rdi);
            //__ push(r19);


            Label slow_path;
            Label retry_tlab, try_eden;
            Label done;
//            __ tlab_refill(retry_tlab, try_eden, slow_path); // does not destroy r3 (klass), returns r5
//
//            __ bind(retry_tlab);
//
//            // get the instance size (size is postive so movl is fine for 64bit)
//            __ ldrw(obj_size, Address(klass, Klass::layout_helper_offset()));
//
//            __ tlab_allocate(obj, obj_size, 0, t1, t2, slow_path);
//
//            __ initialize_object(obj, klass, obj_size, 0, t1, t2);
//            __ verify_oop(obj);
//            __ ldp(r5, r19, Address(__ post(sp, 2 * wordSize)));
//            __ ret(lr);
//
//            __ bind(try_eden);
            // get the instance size (size is postive so movl is fine for 64bit)
            __ ldrw(obj_size, Address(klass, Klass::layout_helper_offset()));

            __ eden_allocate(obj, obj_size, 0, t1, slow_path);
            __ incr_allocated_bytes(rthread, obj_size, 0, rscratch1);

            __ initialize_object(obj, klass, obj_size, 0, t1, t2);
            __ verify_oop(obj);
            __ ldp(r5, r19, Address(__ post(sp, 2 * wordSize)));
            __ ret(lr);

            __ bind(slow_path);
            __ ldp(r5, r19, Address(__ post(sp, 2 * wordSize)));

            //below didn't change

            __ mov(c_rarg1, klass);

            //implementation of call_VME
            //__ call_VME(CAST_FROM_FN_PTR(address, EnclaveMemory::static_vm_new_obj));

            __ mov(c_rarg0, rthread);
            if (true) {
                // TODO: move it to InterpreterMacroAssember
                // quick fix to avoid gc in runtime
                // movptr(Address(rbp, frame::interpreter_frame_bcx_offset * wordSize), r13);
                Register last_java_sp = esp;
                __ lea(last_java_sp, Address(esp, wordSize));
                __ set_last_Java_frame(rthread, last_java_sp, rfp, NULL);
            }
            __ lea(rscratch1, CAST_FROM_FN_PTR(address, EnclaveMemory::static_vm_new_obj));
            __ br(rscratch1);
            if (true) {
                __ reset_last_Java_frame(true);
                // quick fix to avoid gc in runtime
//      movptr(r13, Address(rbp, frame::interpreter_frame_bcx_offset * wordSize));
            }
            if (true) {
                // check for pending exceptions (java_thread is set upon return)
                // r15 is callee-saved
                //compare to NULLWORD using noreg

                __ cmpptr(noreg, Address(rthread, Thread::pending_exception_offset()));

                Label ok;
                __ br(Assembler::EQ, ok);
                //__ b(RuntimeAddress(Interpreter::throw_forward_entry()));
                __ lea(rscratch1, Interpreter::throw_forward_entry());
                __ br(rscratch1);
                __ bind(ok);
            }
            //end of call_VME

            __ verify_oop(r0);
            // continue
            __ bind(done);
            __ leave();
            __ ret(lr);

            break;
        }

         // __ call_VME(CAST_FROM_FN_PTR(address, EnclaveMemory::static_klass_new_array));
        case new_type_array_id:
        {
            Register length   = r19; // Incoming
            Register klass    = r3; // Incoming
            Register obj      = r0; // Result

            Register arr_size = r4;
            Register t1       = r2;
            Register t2       = r5;

            Label slow_path;
            assert_different_registers(length, klass, obj, arr_size, t1, t2);

            // check that array length is small enough for fast path.
            __ mov(rscratch1, max_array_allocation_length);
            __ cmpw(length, rscratch1);
            __ br(Assembler::HI, slow_path);

            // get the allocation size: round_up(hdr + length << (layout_helper & 0x1F))
            // since size is positive ldrw does right thing on 64bit
            __ ldrw(t1, Address(klass, Klass::layout_helper_offset()));
            // since size is postive movw does right thing on 64bit
            __ movw(arr_size, length);
            __ lslvw(arr_size, length, t1);
            __ ubfx(t1, t1, Klass::_lh_header_size_shift,
                    exact_log2(Klass::_lh_header_size_mask + 1));
            __ add(arr_size, arr_size, t1);

            __ add(arr_size, arr_size, MinObjAlignmentInBytesMask); // align up
            __ andr(arr_size, arr_size, ~MinObjAlignmentInBytesMask);

            __ eden_allocate(obj, arr_size, 0, t1, slow_path);  // preserves arr_size
            __ incr_allocated_bytes(rthread, arr_size, 0, rscratch1);

            __ initialize_header(obj, klass, length, t1, t2);
            __ ldrb(t1, Address(klass, in_bytes(Klass::layout_helper_offset()) + (Klass::_lh_header_size_shift / BitsPerByte)));
            assert(Klass::_lh_header_size_shift % BitsPerByte == 0, "bytewise");
            assert(Klass::_lh_header_size_mask <= 0xFF, "bytewise");
            __ andr(t1, t1, Klass::_lh_header_size_mask);
            __ sub(arr_size, arr_size, t1);  // body length
            __ add(t1, t1, obj);       // body start
            __ initialize_body(t1, arr_size, 0, t2);
            __ verify_oop(obj);

            __ ret(lr);

            __ bind(slow_path);

            // keep and change to aarch64 version
            __ ldrw(c_rarg1, Address(r3, Klass::java_mirror_offset()));
            __ ldrw(c_rarg2, length);

            //__ call_VME(CAST_FROM_FN_PTR(address, EnclaveMemory::static_klass_new_array));

            // call_VME implementation

            __ mov(c_rarg0, rthread);
            if (true) {
                // TODO: move it to InterpreterMacroAssember
                // quick fix to avoid gc in runtime
                // movptr(Address(rbp, frame::interpreter_frame_bcx_offset * wordSize), r13);
                Register last_java_sp = esp;
                __ lea(last_java_sp, Address(esp, wordSize));
                __ set_last_Java_frame(rthread, last_java_sp, rfp, NULL);
            }
            //__ b(RuntimeAddress(CAST_FROM_FN_PTR(address, EnclaveMemory::static_klass_new_array)));
            __ lea(rscratch1, CAST_FROM_FN_PTR(address, EnclaveMemory::static_klass_new_array));
            __ br(rscratch1);

            if (true) {
                __ reset_last_Java_frame(true);
                // quick fix to avoid gc in runtime
//      movptr(r13, Address(rbp, frame::interpreter_frame_bcx_offset * wordSize));
            }
            if (true) {
                // check for pending exceptions (java_thread is set upon return)
                // r15 is callee-saved
                //compare to NULLWORD using noreg

                __ cmpptr(noreg, Address(rthread, Thread::pending_exception_offset()));

                Label ok;
                __ br(Assembler::EQ, ok);
                //__ b(RuntimeAddress(Interpreter::throw_forward_entry()));
                __ lea(rscratch1, Interpreter::throw_forward_entry());
                __ br(rscratch1);
                __ bind(ok);
            }

            __ ret(lr);
        }

        // __ call_VME(CAST_FROM_FN_PTR(address, EnclaveMemory::static_klass_obj_array));
        case new_object_array_id:
        {
            Register length   = r19; // Incoming
            Register klass    = r3; // Incoming
            Register obj      = r0; // Result

            Register arr_size = r4;
            Register t1       = r2;
            Register t2       = r5;

            Label slow_path;
            assert_different_registers(length, klass, obj, arr_size, t1, t2);

            // check that array length is small enough for fast path.
            __ mov(rscratch1, max_array_allocation_length);
            __ cmpw(length, rscratch1);
            __ br(Assembler::HI, slow_path);

            // get the allocation size: round_up(hdr + length << (layout_helper & 0x1F))
            // since size is positive ldrw does right thing on 64bit
            __ ldrw(t1, Address(klass, Klass::layout_helper_offset()));
            // since size is postive movw does right thing on 64bit
            __ movw(arr_size, length);
            __ lslvw(arr_size, length, t1);
            __ ubfx(t1, t1, Klass::_lh_header_size_shift,
                    exact_log2(Klass::_lh_header_size_mask + 1));
            __ add(arr_size, arr_size, t1);
            __ add(arr_size, arr_size, MinObjAlignmentInBytesMask); // align up
            __ andr(arr_size, arr_size, ~MinObjAlignmentInBytesMask);

            __ eden_allocate(obj, arr_size, 0, t1, slow_path);  // preserves arr_size
            __ incr_allocated_bytes(rthread, arr_size, 0, rscratch1);

            __ initialize_header(obj, klass, length, t1, t2);
            __ ldrb(t1, Address(klass, in_bytes(Klass::layout_helper_offset()) + (Klass::_lh_header_size_shift / BitsPerByte)));
            assert(Klass::_lh_header_size_shift % BitsPerByte == 0, "bytewise");
            assert(Klass::_lh_header_size_mask <= 0xFF, "bytewise");
            __ andr(t1, t1, Klass::_lh_header_size_mask);
            __ sub(arr_size, arr_size, t1);  // body length
            __ add(t1, t1, obj);       // body start
            __ initialize_body(t1, arr_size, 0, t2);
            __ verify_oop(obj);

            __ ret(lr);

            __ bind(slow_path);

            // keep and change to aarch64 version
            __ movptr(c_rarg1, r3);
            __ ldrw(c_rarg2, length);

            //__ call_VME(CAST_FROM_FN_PTR(address, EnclaveMemory::static_klass_obj_array));

            // call_VME implementation

            __ mov(c_rarg0, rthread);
            if (true) {
                // TODO: move it to InterpreterMacroAssember
                // quick fix to avoid gc in runtime
                // movptr(Address(rbp, frame::interpreter_frame_bcx_offset * wordSize), r13);
                Register last_java_sp = esp;
                __ lea(last_java_sp, Address(esp, wordSize));
                __ set_last_Java_frame(rthread, last_java_sp, rfp, NULL);
            }
            //__ b(RuntimeAddress(CAST_FROM_FN_PTR(address, EnclaveMemory::static_klass_obj_array)));
            __ lea(rscratch1, CAST_FROM_FN_PTR(address, EnclaveMemory::static_klass_obj_array));
            __ br(rscratch1);
            if (true) {
                __ reset_last_Java_frame(true);
                // quick fix to avoid gc in runtime
//      movptr(r13, Address(rbp, frame::interpreter_frame_bcx_offset * wordSize));
            }
            if (true) {
                // check for pending exceptions (java_thread is set upon return)
                // r15 is callee-saved
                //compare to NULLWORD using noreg

                __ cmpptr(noreg, Address(rthread, Thread::pending_exception_offset()));

                Label ok;
                __ br(Assembler::EQ, ok);
                //__ b(RuntimeAddress(Interpreter::throw_forward_entry()));
                __ lea(rscratch1, Interpreter::throw_forward_entry());
                __ br(rscratch1);
                __ bind(ok);
            }

            __ ret(lr);
        }

        case access_field_patching_id:
        { StubFrame f(sasm, "access_field_patching", dont_gc_arguments);
            // we should set up register map
            generate_patching(sasm, CAST_FROM_FN_PTR(address, access_field_patching));
        }
            break;

        case load_klass_patching_id:
        { StubFrame f(sasm, "load_klass_patching", dont_gc_arguments);
            // we should set up register map
            generate_patching(sasm, CAST_FROM_FN_PTR(address, move_klass_patching));
        }
            break;

        case load_mirror_patching_id:
        { StubFrame f(sasm, "load_mirror_patching", dont_gc_arguments);
            // we should set up register map
            generate_patching(sasm, CAST_FROM_FN_PTR(address, move_mirror_patching));
        }
            break;

        case load_appendix_patching_id:
        { StubFrame f(sasm, "load_appendix_patching", dont_gc_arguments);
            // we should set up register map
            generate_patching(sasm, CAST_FROM_FN_PTR(address, move_appendix_patching));
        }
            break;

        case load_method_patching_id:
        {
            StubFrame f(sasm, "load_method_patching", dont_gc_arguments);
            generate_patching(sasm, CAST_FROM_FN_PTR(address, move_method_patching));
        }
            break;


        case compile_method_patching_id:
        {
            Label compile_start;
            __ str(r0, Address(rfp, Method::from_compiled_offset()));
            //and operation and test zero
            __ andr(r0, r0, r0);
            __ cbz(r0, compile_start);
            __ ret(lr);
            __ bind(compile_start);
            __ str(rfp, Address(rthread, JavaThread::compiled_method_offset()));
            {
                StubFrame f(sasm, "compile_method_patching", dont_gc_arguments);
                generate_patching(sasm, CAST_FROM_FN_PTR(address, compile_method_patching));
            }
            break;
        }

        case gc_barrier_id: {
            {
                //
                //__ call_VME(CAST_FROM_FN_PTR(address, gc_barrier));
                // call_VME implementation

                __ mov(c_rarg0, rthread);
                if (true) {
                    // TODO: move it to InterpreterMacroAssember
                    // quick fix to avoid gc in runtime
                    // movptr(Address(rbp, frame::interpreter_frame_bcx_offset * wordSize), r13);
                    Register last_java_sp = esp;
                    __ lea(last_java_sp, Address(esp, wordSize));
                    __ set_last_Java_frame(rthread, last_java_sp, rfp, NULL);
                }
                __ b(RuntimeAddress(CAST_FROM_FN_PTR(address, gc_barrier)));
                if (true) {
                    __ reset_last_Java_frame(true);
                    // quick fix to avoid gc in runtime
//      movptr(r13, Address(rbp, frame::interpreter_frame_bcx_offset * wordSize));
                }
                if (true) {
                    // check for pending exceptions (java_thread is set upon return)
                    // r15 is callee-saved
                    //compare to NULLWORD using noreg

                    __ cmpptr(noreg, Address(rthread, Thread::pending_exception_offset()));

                    Label ok;
                    __ br(Assembler::EQ, ok);
                    __ b(RuntimeAddress(Interpreter::throw_forward_entry()));
                    __ bind(ok);
                }

                __ ret(lr);
            }
            break;
        }

        default:
        {
//            StubFrame f(sasm, "unimplemented entry", dont_gc_arguments);
//            __ movptr(rax, (int)id);
//            __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, unimplemented_entry), rax);
//            __ should_not_reach_here();
        }
            break;
    }

}

void Runtime0::initialize() {
    int code_buffer_size = JCompiler::desired_max_constant_size() * 10 +
            JCompiler::desired_max_constant_size();

    BufferBlob* buffer_blob = BufferBlob::create("C0 temporary CodeBuffer", code_buffer_size);

    initialize_pd();
    // generate stubs
    for (int id = 0; id < number_of_ids; id++) generate_blob_for(buffer_blob, (StubID)id);
}

void Runtime0::initialize_pd() {
    /*
    int i;
    int sp_offset = 0;

    // all float registers are saved explicitly
    assert(FrameMap::nof_fpu_regs == 32, "double registers not handled here");
    for (i = 0; i < FrameMap::nof_fpu_regs; i++) {
        fpu_reg_save_offsets[i] = sp_offset;
        sp_offset += 2;   // SP offsets are in halfwords
    }

    for (i = 0; i < FrameMap::nof_cpu_regs; i++) {
        Register r = as_Register(i);
        cpu_reg_save_offsets[i] = sp_offset;
        sp_offset += 2;   // SP offsets are in halfwords
    }
     */
}

void Runtime0::generate_patching(StubAssembler *sasm, address target) {
    // use the maximum number of runtime-arguments here because it is difficult to
    // distinguish each RT-Call.
    // Note: This number affects also the RT-Call in generate_handle_exception because
    //       the oop-map is shared for all calls.
    const int num_rt_args = 2;  // thread + dummy

    save_live_registers(sasm, num_rt_args);

    const Register thread = rthread;
    // No need to worry about dummy
    __ mov(c_rarg0, thread);

    //__ set_last_Java_frame(thread, noreg, rbp, NULL);
    // to below
    Label retaddr;
    __ set_last_Java_frame(esp, rfp, retaddr, rscratch1);

    // do the call
    //__ call(RuntimeAddress(target));
    // to below
    __ lea(rscratch1, RuntimeAddress(target));
    __ blr(rscratch1);
    __ bind(retaddr);

    // verify callee-saved register

    //__ reset_last_Java_frame(thread, true);
    // to below
    __ reset_last_Java_frame(true);
    __ maybe_isb();

    // check for pending exceptions
    { Label L;
        __ ldr(rscratch1, Address(rthread, Thread::pending_exception_offset()));
        __ cbz(rscratch1, L);
        // exception pending => remove activation and forward to exception handler
        // TODO: fix me exception

        // the deopt blob expects exceptions in the special fields of
        // JavaThread, so copy and clear pending exception.

        // load and clear pending exception
        __ ldr(r0, Address(rthread, Thread::pending_exception_offset()));
        __ str(zr, Address(rthread, Thread::pending_exception_offset()));

        // check that there is really a valid exception
        //comment out because no such function
        //__ verify_not_null_oop(r0);

        // load throwing pc: this is the return address of the stub
        __ mov(r3, lr);

        // store exception oop and throwing pc to JavaThread
        __ str(r0, Address(rthread, JavaThread::exception_oop_offset()));
        __ str(r3, Address(rthread, JavaThread::exception_pc_offset()));

        restore_live_registers(sasm);

        __ leave();

        // Forward the exception directly to deopt blob. We can blow no
        // registers and must leave throwing pc on the stack.  A patch may
        // have values live in registers so the entry point with the
        // exception in tls.

        //comment out because no deopt_blob

        //__ far_jump(RuntimeAddress(deopt_blob->unpack_with_exception_in_tls()));

        __ bind(L);
    }

    restore_live_registers(sasm);
//    __ leave();
    // __ ret(0);
}

int Runtime0::move_method_patching(JavaThread *thread) {
    patch_code(thread, load_method_patching_id);
}

int Runtime0::access_field_patching(JavaThread *thread) {
    patch_code(thread, access_field_patching_id);
}

int Runtime0::move_klass_patching(JavaThread *thread) {
    patch_code(thread, load_klass_patching_id);
}

int Runtime0::move_mirror_patching(JavaThread *thread) {
    patch_code(thread, load_mirror_patching_id);
}

int Runtime0::move_appendix_patching(JavaThread *thread) {
    patch_code(thread, load_appendix_patching_id);
}

void Runtime0::patch_code(JavaThread *thread, Runtime0::StubID stub_id) {
    NOT_PRODUCT(_patch_code_slowcase_cnt++;)

    JavaThread* THREAD = thread;

    ResourceMark rm(thread);
    RegisterMap reg_map(thread, false);
    frame runtime_frame = thread->last_frame();
    frame caller_frame = runtime_frame.sender(&reg_map);

    // jianyu: we pass esp to sp of runtime_frame for now, but this seems ugly
    caller_frame.set_sp(runtime_frame.sp());

    // last java frame on stack

    Method* mth;

    methodHandle caller_method(THREAD, caller_frame.interpreter_frame_method());
    // Note that caller_method->code() may not be same as caller_code because of OSR's
    // Note also that in the presence of inlining it is not guaranteed
    // that caller_method() == caller_code->method()

    int info_offset = NativeJump::instruction_size // blr
                    + NativeMovConstReg::instruction_size // lea 
                    + NativeInstruction::instruction_size // nop
                    + NativeGeneralJump::instruction_size; // fake jump addr

    NativeGeneralJump *bci_jmp = nativeGeneralJump_at(caller_frame.pc() - info_offset);
    
    int full_int = bci_jmp->jump_destination();
    int bci = (full_int & 0xffff);
    int bytes_to_copy = (full_int >> 16);

    int copy_offset = info_offset 
                    + NativeInstruction::instruction_size
                    + bytes_to_copy;

    // printf("bci %lx %lx %d, %d %lx\n", full_int,caller_frame.pc() - info_offset, bci, bytes_to_copy, caller_frame.pc() - copy_offset);

    Bytecodes::Code code = caller_method()->java_code_at(bci);

    bool deoptimize_for_volatile = false;
    int patch_field_offset = -1;
    KlassHandle init_klass(THREAD, NULL); // klass needed by load_klass_patching code
    KlassHandle load_klass(THREAD, NULL); // klass needed by load_klass_patching code
    Handle mirror(THREAD, NULL);                    // oop needed by load_mirror_patching code
    Handle appendix(THREAD, NULL);                  // oop needed by appendix_patching code
    Klass* interface_klass;
    int interface_itable_idx;
    int virtual_vtable_idx;
    bool load_klass_or_mirror_patch_id =
            (stub_id == Runtime0::load_klass_patching_id || stub_id == Runtime0::load_mirror_patching_id);

    if (stub_id == Runtime0::access_field_patching_id) {
        Bytecode_field field_access(caller_method, bci);
        fieldDescriptor result; // initialize class if needed
        Bytecodes::Code code = field_access.code();
        constantPoolHandle constants(THREAD, caller_method->constants());
        LinkResolver::resolve_field_access(result, constants, field_access.index(), Bytecodes::java_code(code), CHECK);
        patch_field_offset = result.offset();

        // If we're patching a field which is volatile then at compile it
        // must not have been know to be volatile, so the generated code
        // isn't correct for a volatile reference.  The nmethod has to be
        // deoptimized so that the code can be regenerated correctly.
        // This check is only needed for access_field_patching since this
        // is the path for patching field offsets.  load_klass is only
        // used for patching references to oops which don't need special
        // handling in the volatile case.
//        deoptimize_for_volatile = result.access_flags().is_volatile();

    } else if (load_klass_or_mirror_patch_id) {
        Klass* k = NULL;
        switch (code) {
            case Bytecodes::_putstatic:
            case Bytecodes::_getstatic:
            { Klass* klass = resolve_field_return_klass(caller_method, bci, CHECK);
                init_klass = KlassHandle(THREAD, klass);
                mirror = Handle(THREAD, (oop)klass);
            }
                break;
            case Bytecodes::_new:
            { Bytecode_new bnew(caller_method(), caller_method->bcp_from(bci));
                k = caller_method->constants()->klass_at(bnew.index(), CHECK);
            }
                break;
            case Bytecodes::_multianewarray:
            { Bytecode_multianewarray mna(caller_method(), caller_method->bcp_from(bci));
                k = caller_method->constants()->klass_at(mna.index(), CHECK);
            }
                break;
            case Bytecodes::_instanceof:
            { Bytecode_instanceof io(caller_method(), caller_method->bcp_from(bci));
                k = caller_method->constants()->klass_at(io.index(), CHECK);
            }
                break;
            case Bytecodes::_checkcast:
            { Bytecode_checkcast cc(caller_method(), caller_method->bcp_from(bci));
                k = caller_method->constants()->klass_at(cc.index(), CHECK);
            }
                break;
            case Bytecodes::_anewarray:
            { Bytecode_anewarray anew(caller_method(), caller_method->bcp_from(bci));
                Klass* ek = caller_method->constants()->klass_at(anew.index(), CHECK);
                k = ek->array_klass(CHECK);
            }
                break;
            case Bytecodes::_ldc:
            case Bytecodes::_ldc_w:
            {
                Bytecode_loadconstant cc(caller_method, bci);
                oop m = cc.resolve_constant(CHECK);
                mirror = Handle(THREAD, m);
            }
                break;
            default: fatal("unexpected bytecode for load_klass_or_mirror_patch_id");
        }
        // convert to handle
        load_klass = KlassHandle(THREAD, k);
    } else if (stub_id == load_method_patching_id) {
        Bytecode_invoke call(caller_method, bci);
        Bytecodes::Code bytecode = call.invoke_code();
        int bytecode_index = call.index();

        oop recv = NULL;
        if (bytecode == Bytecodes::_invokevirtual || bytecode == Bytecodes::_invokeinterface ||
            bytecode == Bytecodes::_invokespecial) {
            recv = caller_frame.interpreter_callee_receiver(call.signature());
            if (recv == NULL) {
              THROW(vmSymbols::java_lang_NullPointerException());
            }
        }
        // Resolve method. This is parameterized by bytecode.
        constantPoolHandle constants(THREAD, caller_method->constants());
        CallInfo callinfo;
        LinkResolver::resolve_invoke(callinfo, Handle(recv), constants, bytecode_index, bytecode, THREAD, (recv == NULL)? NULL : recv->klass());
        if (bytecode == Bytecodes::_invokeinterface || bytecode == Bytecodes::_invokevirtual) {
            mth = callinfo.resolved_method()();
        } else {
            mth = callinfo.selected_method()();
        }
        EnclaveRuntime::compile_method(mth);
    } else if (stub_id == compile_method_patching_id) {
        Method* compiled_method = thread->compiled_method();
        EnclaveRuntime::compile_method(compiled_method);
    } else if (stub_id == load_appendix_patching_id) {
        D_WARN_Unimplement;
    } else {
        ShouldNotReachHere();
    }

    if (deoptimize_for_volatile) {
        D_WARN_Unimplement;
    }

    // Now copy code back
    // we are encountering the call interface, we cannot patch the stub
    if (stub_id == compile_method_patching_id && bci == 1) {
        // no patch is necessary for invokeinterface
        return;
    }

    {
        // Deoptimization may have happened while we waited for the lock.
        // In that case we don't bother to do any patching we just return
        // and let the deopt happen

        NativeGeneralJump* jump = nativeGeneralJump_at(caller_frame.pc());
        address instr_pc = jump->jump_destination();
        NativeInstruction* ni = nativeInstruction_at(instr_pc + 12);
        // if (ni->is_jump() ) {
            // the jump has not been patched yet
            // The jump destination is slow case and therefore not part of the stubs
            // (stubs are only for StaticCalls)

            // format of buffer
            //    ....
            //    instr byte 0     <-- copy_buff
            //    instr byte 1
            //    ..
            //    instr byte n-1
            //      n
            //    ....             <-- call destination

            address copy_buff = caller_frame.pc() - copy_offset;
            address being_initialized_entry = caller_frame.pc();
            // depending on the code below, do_patch says whether to copy the patch body back into the nmethod
            bool do_patch = true;
            if (stub_id == Runtime0::access_field_patching_id) {
                // The offset may not be correct if the class was not loaded at code generation time.
                // Set it now.
                NativeMemOffset* n_move = nativeMemOffset_at(copy_buff);
                assert(n_move->offset() == 0 || (n_move->offset() == 4 && (patch_field_type == T_DOUBLE || patch_field_type == T_LONG)), "illegal offset for type");
                assert(patch_field_offset >= 0, "illegal offset");
                n_move->set_offset(patch_field_offset);
            } else if (load_klass_or_mirror_patch_id) {
                // If a getstatic or putstatic is referencing a klass which
                // isn't fully initialized, the patch body isn't copied into
                // place until initialization is complete.  In this case the
                // patch site is setup so that any threads besides the
                // initializing thread are forced to come into the VM and
                // block.
                do_patch = (code != Bytecodes::_getstatic && code != Bytecodes::_putstatic) ||
                           InstanceKlass::cast(init_klass())->is_initialized();
                NativeGeneralJump* jump = nativeGeneralJump_at(instr_pc);
                if (jump->jump_destination() == being_initialized_entry) {
                    assert(do_patch == true, "initialization must be complete at this point");
                } else {
                    // patch the instruction <move reg, klass>
                    NativeMovConstReg* n_copy = nativeMovConstReg_at(copy_buff);

                    assert(n_copy->data() == 0 ||
                           n_copy->data() == (intptr_t)Universe::non_oop_word(),
                           "illegal init value");
                    if (stub_id == Runtime0::load_klass_patching_id) {
                        assert(load_klass() != NULL, "klass not set");
                        n_copy->set_data((intx) (load_klass()));
                    } else {
                        assert(mirror() != NULL, "klass not set");
                        // Don't need a G1 pre-barrier here since we assert above that data isn't an oop.
                        n_copy->set_data(cast_from_oop<intx>(mirror()));
                    }

                }
            } else if (stub_id == Runtime0::load_method_patching_id) {
                if (code == Bytecodes::_invokeinterface) {
                    NativeMovConstReg *n_copy = nativeMovConstReg_at(copy_buff);
                    n_copy->set_data((intptr_t)interface_klass);
                    NativeMovConstReg *n_copy_index = nativeMovConstReg_at(copy_buff + NativeMovConstReg::instruction_size);
                    n_copy_index->set_data(interface_itable_idx);
                } else if (code == Bytecodes::_invokevirtual && virtual_vtable_idx > 0) {
                    NativeMovConstReg *n_copy = nativeMovConstReg_at(copy_buff);
                    n_copy->set_data(virtual_vtable_idx);
                    unsigned char* jmp_buf = copy_buff + NativeMovConstReg::instruction_size;
                    if (jmp_buf[0] != 0xEB) {
                        int32_t* jmp_offset = (int32_t*)(jmp_buf + 1);
                        *jmp_offset = 0;
                    } else {
                        jmp_buf[1] = 0;
                    }
                } else {
                    NativeMovConstReg *n_copy = nativeMovConstReg_at(copy_buff);
                    n_copy->set_data((intptr_t)mth);
                }
            } else if (stub_id == Runtime0::load_appendix_patching_id) {
                NativeMovConstReg* n_copy = nativeMovConstReg_at(copy_buff);
                assert(n_copy->data() == 0 ||
                       n_copy->data() == (intptr_t)Universe::non_oop_word(),
                       "illegal init value");
                n_copy->set_data(cast_from_oop<intx>(appendix()));

            } else if (stub_id == Runtime0::compile_method_patching_id) {
                // reset as nop
                for (int i = 0;i < NativeCall::instruction_size;i++) {
                    copy_buff[i] = 0x90;
                }
            } else {
                ShouldNotReachHere();
            }

            if (do_patch) {
                // replace instructions
                // first replace the tail, then the call

                for (int i = 0; i < bytes_to_copy; i++) {
                    address ptr = copy_buff + i;
                    int a_byte = (*ptr) & 0xFF;
                    address dst = instr_pc + i;
                    *(unsigned char*)dst = (unsigned char) a_byte;
                }
                ICache::invalidate_range(instr_pc, bytes_to_copy);
                // NativeGeneralJump::replace_mt_safe(instr_pc, copy_buff);

                if (load_klass_or_mirror_patch_id ||
                    stub_id == Runtime0::load_appendix_patching_id) {
                }

            } else {
                ICache::invalidate_range(copy_buff, bytes_to_copy);
                NativeGeneralJump::insert_unconditional(instr_pc, being_initialized_entry);
            }
        }
    // }

    // If we are patching in a non-perm oop, make sure the nmethod
    // is on the right list.
    // TODO: gc

}

int Runtime0::compile_method_patching(JavaThread *thread) {
    patch_code(thread, compile_method_patching_id);
}

void Runtime0::gc_barrier(JavaThread *thread) {
    EnclaveGC::atomic_spin_gc();
}
