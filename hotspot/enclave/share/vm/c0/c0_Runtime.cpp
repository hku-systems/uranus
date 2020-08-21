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

    constantPoolHandle constants(THREAD, caller->constants());
    return (Klass*)Klass_resolve_get_put(THREAD, caller(), field_access.index(), Bytecodes::java_code(code));
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
//TODO: change back to Uranus x86
void Runtime0::generate_code_for(Runtime0::StubID id, StubAssembler *sasm) {
    // for better readability
    const bool must_gc_arguments = true;
    const bool dont_gc_arguments = false;

    // default value; overwritten for some optimized stubs that are called from methods that do not use the fpu
    bool save_fpu_registers = true;

    // stub code & info for the different stubs
    OopMapSet* oop_maps = NULL;
    switch (id) {
        /*
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


            __ push(rdi);
            __ push(r19);

            Label slow_case;
            Label done;
            Label initialize_header;
            Label initialize_object; // including clearing the fields
            Label allocate_shared;

            // get instance_size in InstanceKlass (scaled to a count of bytes)
            __ movl(obj_size,
                    Address(klass,
                            Klass::layout_helper_offset()));
            // test to see if it has a finalizer or is malformed in some way
            __ testl(obj_size, Klass::_lh_instance_slow_path_bit);
            __ jcc(Assembler::notZero, slow_case);

            // Allocate the instance
            // 1) Try to allocate in the TLAB
            // 2) if fail and the object is large allocate in the shared Eden
            // 3) if the above fails (or is not applicable), go to a slow case
            // (creates a new TLAB, etc.)

            // Allocation in the shared Eden, if allowed.
            //
            // rdx: instance size in bytes

            __ bind(allocate_shared);

            __ eden_allocate(obj, obj_size, 0, t1, slow_case);

            __ initialize_object(obj, klass, obj_size, 0, t1, t2);
            __ verify_oop(obj);
            __ pop(rbx);
            __ pop(rdi);
            __ ret(0);

            // slow case
            __ bind(slow_case);
            __ pop(rbx);
            __ pop(rdi);

            __ mov(c_rarg1, klass);
            __ call_VME(CAST_FROM_FN_PTR(address, EnclaveMemory::static_vm_new_obj));
            __ verify_oop(rax);
            // continue
            __ bind(done);
            __ ret(0);

            break;
        }
        */
        /*
        case new_type_array_id:
        {
            Register length = rbx;
            Register klass  = rdx;
            Register obj    = rax;

            Register arr_size = rsi;
            Register t1     = rcx;
            Register t2     = rdi;
            Label slow_path;
            assert_different_registers(length, klass, obj, arr_size, t1, t2);
            __ cmpl(length, max_array_allocation_length);
            __ jcc(Assembler::above, slow_path);

            // get the allocation size: round_up(hdr + length << (layout_helper & 0x1F))
            // since size is positive movl does right thing on 64bit
            __ movl(t1, Address(klass, Klass::layout_helper_offset()));
            // since size is postive movl does right thing on 64bit
            __ movl(arr_size, length);
            assert(t1 == rcx, "fixed register usage");
            __ shlptr(arr_size );
            // by t1=rcx, mod 32
            __ shrptr(t1, Klass::_lh_header_size_shift);
            __ andptr(t1, Klass::_lh_header_size_mask);
            __ addptr(arr_size, t1);
            __ addptr(arr_size, MinObjAlignmentInBytesMask); // align up
            __ andptr(arr_size, ~MinObjAlignmentInBytesMask);

            __ eden_allocate(obj, arr_size, 0, t1, slow_path);  // preserves arr_size

            __ initialize_header(obj, klass, length, t1, t2);
            __ movb(t1, Address(klass, in_bytes(Klass::layout_helper_offset()) + (Klass::_lh_header_size_shift / BitsPerByte)));
            assert(Klass::_lh_header_size_shift % BitsPerByte == 0, "bytewise");
            assert(Klass::_lh_header_size_mask <= 0xFF, "bytewise");
            __ andptr(t1, Klass::_lh_header_size_mask);
            __ subptr(arr_size, t1);  // body length
            __ addptr(t1, obj);       // body start
            __ initialize_body(t1, arr_size, 0, t2);
            __ verify_oop(obj);
            __ ret(0);

            __ bind(slow_path);

            __ movptr(c_rarg1, Address(rdx, Klass::java_mirror_offset()));
            __ movl(c_rarg2, length);
            __ call_VME(CAST_FROM_FN_PTR(address, EnclaveMemory::static_klass_new_array));
            __ ret(0);
        }
         */
        /*
        case new_object_array_id:
        {
            Register length = rbx;
            Register klass  = rdx;
            Register obj    = rax;

            Register arr_size = rsi;
            Register t1     = rcx;
            Register t2     = rdi;
            Label slow_path;
            assert_different_registers(length, klass, obj, arr_size, t1, t2);
            __ cmpl(length, max_array_allocation_length);
            __ jcc(Assembler::above, slow_path);

            // get the allocation size: round_up(hdr + length << (layout_helper & 0x1F))
            // since size is positive movl does right thing on 64bit
            __ movl(t1, Address(klass, Klass::layout_helper_offset()));
            // since size is postive movl does right thing on 64bit
            __ movl(arr_size, length);
            assert(t1 == rcx, "fixed register usage");
            __ shlptr(arr_size );
            //by t1=rcx, mod 32
            __ shrptr(t1, Klass::_lh_header_size_shift);
            __ andptr(t1, Klass::_lh_header_size_mask);
            __ addptr(arr_size, t1);
            __ addptr(arr_size, MinObjAlignmentInBytesMask); // align up
            __ andptr(arr_size, ~MinObjAlignmentInBytesMask);

            __ eden_allocate(obj, arr_size, 0, t1, slow_path);  // preserves arr_size

            __ initialize_header(obj, klass, length, t1, t2);
            __ movb(t1, Address(klass, in_bytes(Klass::layout_helper_offset()) + (Klass::_lh_header_size_shift / BitsPerByte)));
            assert(Klass::_lh_header_size_shift % BitsPerByte == 0, "bytewise");
            assert(Klass::_lh_header_size_mask <= 0xFF, "bytewise");
            __ andptr(t1, Klass::_lh_header_size_mask);
            __ subptr(arr_size, t1);  // body length
            __ addptr(t1, obj);       // body start
            __ initialize_body(t1, arr_size, 0, t2);
            __ verify_oop(obj);
            __ ret(0);

            __ bind(slow_path);

            __ movptr(c_rarg1, rdx);
            __ movl(c_rarg2, length);
            __ call_VME(CAST_FROM_FN_PTR(address, EnclaveMemory::static_klass_obj_array));
            __ ret(0);
        }
        */
        /*
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
        */
        case load_method_patching_id:
        { StubFrame f(sasm, "load_method_patching", dont_gc_arguments);
            generate_patching(sasm, CAST_FROM_FN_PTR(address, move_method_patching));
        }
            break;
        /*
        case compile_method_patching_id:
        {
            Label compile_start;
            __ movptr(rax, Address(rbx, Method::enclave_native_function_offset()));
            __ testptr(rax, rax);
            __ jcc(MacroAssembler::zero, compile_start);
            __ ret(0);
            __ bind(compile_start);
            __ movptr(Address(r15_thread, JavaThread::compiled_method_offset()), rbx);
            {
                StubFrame f(sasm, "compile_method_patching", dont_gc_arguments);
                generate_patching(sasm, CAST_FROM_FN_PTR(address, compile_method_patching));
            }
            break;
        }
        case gc_barrier_id: {
            {
                //
                __ call_VME(CAST_FROM_FN_PTR(address, gc_barrier));
                __ ret(0);
            }
            break;
        }
         */
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
    __ set_last_Java_frame(sp, rfp, retaddr, rscratch1);

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
        __ verify_not_null_oop(r0);

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
        __ far_jump(RuntimeAddress(deopt_blob->unpack_with_exception_in_tls()));

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
//TODO: change back to Uranus x86
void Runtime0::patch_code(JavaThread *thread, Runtime0::StubID stub_id) {
    RegisterMap reg_map(thread, false);

    NOT_PRODUCT(_patch_code_slowcase_cnt++;)
    // According to the ARMv8 ARM, "Concurrent modification and
    // execution of instructions can lead to the resulting instruction
    // performing any behavior that can be achieved by executing any
    // sequence of instructions that can be executed from the same
    // Exception level, except where the instruction before
    // modification and the instruction after modification is a B, BL,
    // NOP, BKPT, SVC, HVC, or SMC instruction."
    //
    // This effectively makes the games we play when patching
    // impossible, so when we come across an access that needs
    // patching we must deoptimize.

    /*
    if (TracePatching) {
        tty->print_cr("Deoptimizing because patch is needed");
    }
     */

    frame runtime_frame = thread->last_frame();
    frame caller_frame = runtime_frame.sender(&reg_map);

    // It's possible the nmethod was invalidated in the last
    // safepoint, but if it's still alive then make it not_entrant.
    /*
    nmethod* nm = CodeCache::find_nmethod(caller_frame.pc());
    if (nm != NULL) {
        nm->make_not_entrant();
    }
     */

    //Deoptimization::deoptimize_frame(thread, caller_frame.id());

    // Return to the now deoptimized frame.
}

int Runtime0::compile_method_patching(JavaThread *thread) {
    patch_code(thread, compile_method_patching_id);
}

void Runtime0::gc_barrier(JavaThread *thread) {
    EnclaveGC::atomic_spin_gc();
}
