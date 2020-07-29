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

static void save_live_registers(StubAssembler* sasm, int num_rt_args,
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

    return generate_oop_map(sasm, save_fpu_registers);
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
       _sasm->ret(0);
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
    const Register exception_oop = r0;
    const Register exception_pc  = r3;

    // for better readability
    const bool must_gc_arguments = true;
    const bool dont_gc_arguments = false;

    // default value; overwritten for some optimized stubs that are called from methods that do not use the fpu
    bool save_fpu_registers = true;

    // stub code & info for the different stubs
    OopMapSet* oop_maps = NULL;
    OopMap* oop_map = NULL;
    switch (id) {
        {
            case forward_exception_id:
            {
                oop_maps = generate_handle_exception(id, sasm);
                __ leave();
                __ ret(lr);
            }
            break;

            case throw_div0_exception_id:
            { StubFrame f(sasm, "throw_div0_exception", dont_gc_arguments);
                oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_div0_exception), false);
            }
            break;

            case throw_null_pointer_exception_id:
            { StubFrame f(sasm, "throw_null_pointer_exception", dont_gc_arguments);
                oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_null_pointer_exception), false);
            }
            break;

            case new_instance_id:
            case fast_new_instance_id:
            case fast_new_instance_init_check_id:
            {
                Register klass = r3; // Incoming
                Register obj   = r0; // Result

                if (id == new_instance_id) {
                    __ set_info("new_instance", dont_gc_arguments);
                } else if (id == fast_new_instance_id) {
                    __ set_info("fast new_instance", dont_gc_arguments);
                } else {
                    assert(id == fast_new_instance_init_check_id, "bad StubID");
                    __ set_info("fast new_instance init check", dont_gc_arguments);
                }

                if ((id == fast_new_instance_id || id == fast_new_instance_init_check_id) &&
                    UseTLAB && FastTLABRefill) {
                    Label slow_path;
                    Register obj_size = r2;
                    Register t1       = r19;
                    Register t2       = r4;
                    assert_different_registers(klass, obj, obj_size, t1, t2);

                    __ stp(r5, r19, Address(__ pre(sp, -2 * wordSize)));

                    if (id == fast_new_instance_init_check_id) {
                        // make sure the klass is initialized
                        __ ldrb(rscratch1, Address(klass, InstanceKlass::init_state_offset()));
                        __ cmpw(rscratch1, InstanceKlass::fully_initialized);
                        __ br(Assembler::NE, slow_path);
                    }

#ifdef ASSERT
                    // assert object can be fast path allocated
          {
            Label ok, not_ok;
            __ ldrw(obj_size, Address(klass, Klass::layout_helper_offset()));
            __ cmp(obj_size, 0u);
            __ br(Assembler::LE, not_ok);  // make sure it's an instance (LH > 0)
            __ tstw(obj_size, Klass::_lh_instance_slow_path_bit);
            __ br(Assembler::EQ, ok);
            __ bind(not_ok);
            __ stop("assert(can be fast path allocated)");
            __ should_not_reach_here();
            __ bind(ok);
          }
#endif // ASSERT

                    // if we got here then the TLAB allocation failed, so try
                    // refilling the TLAB or allocating directly from eden.
                    Label retry_tlab, try_eden;
                    __ tlab_refill(retry_tlab, try_eden, slow_path); // does not destroy r3 (klass), returns r5

                    __ bind(retry_tlab);

                    // get the instance size (size is postive so movl is fine for 64bit)
                    __ ldrw(obj_size, Address(klass, Klass::layout_helper_offset()));

                    __ tlab_allocate(obj, obj_size, 0, t1, t2, slow_path);

                    __ initialize_object(obj, klass, obj_size, 0, t1, t2);
                    __ verify_oop(obj);
                    __ ldp(r5, r19, Address(__ post(sp, 2 * wordSize)));
                    __ ret(lr);

                    __ bind(try_eden);
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
                }

                __ enter();
                OopMap* map = save_live_registers(sasm);
                int call_offset = __ call_RT(obj, noreg, CAST_FROM_FN_PTR(address, new_instance), klass);
                oop_maps = new OopMapSet();
                oop_maps->add_gc_map(call_offset, map);
                restore_live_registers_except_r0(sasm);
                __ verify_oop(obj);
                __ leave();
                __ ret(lr);

                // r0,: new instance
            }

            break;

            case counter_overflow_id:
            {
                Register bci = r0, method = r1;
                __ enter();
                OopMap* map = save_live_registers(sasm);
                // Retrieve bci
                __ ldrw(bci, Address(rfp, 2*BytesPerWord));
                // And a pointer to the Method*
                __ ldr(method, Address(rfp, 3*BytesPerWord));
                int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, counter_overflow), bci, method);
                oop_maps = new OopMapSet();
                oop_maps->add_gc_map(call_offset, map);
                restore_live_registers(sasm);
                __ leave();
                __ ret(lr);
            }
            break;

            case new_type_array_id:
            case new_object_array_id:
            {
                Register length   = r19; // Incoming
                Register klass    = r3; // Incoming
                Register obj      = r0; // Result

                if (id == new_type_array_id) {
                    __ set_info("new_type_array", dont_gc_arguments);
                } else {
                    __ set_info("new_object_array", dont_gc_arguments);
                }

#ifdef ASSERT
                // assert object type is really an array of the proper kind
        {
          Label ok;
          Register t0 = obj;
          __ ldrw(t0, Address(klass, Klass::layout_helper_offset()));
          __ asrw(t0, t0, Klass::_lh_array_tag_shift);
          int tag = ((id == new_type_array_id)
                     ? Klass::_lh_array_tag_type_value
                     : Klass::_lh_array_tag_obj_value);
	  __ mov(rscratch1, tag);
          __ cmpw(t0, rscratch1);
          __ br(Assembler::EQ, ok);
          __ stop("assert(is an array klass)");
          __ should_not_reach_here();
          __ bind(ok);
        }
#endif // ASSERT

                if (UseTLAB && FastTLABRefill) {
                    Register arr_size = r4;
                    Register t1       = r2;
                    Register t2       = r5;
                    Label slow_path;
                    assert_different_registers(length, klass, obj, arr_size, t1, t2);

                    // check that array length is small enough for fast path.
                    __ mov(rscratch1, C1_MacroAssembler::max_array_allocation_length);
                    __ cmpw(length, rscratch1);
                    __ br(Assembler::HI, slow_path);

                    // if we got here then the TLAB allocation failed, so try
                    // refilling the TLAB or allocating directly from eden.
                    Label retry_tlab, try_eden;
                    const Register thread =
                            __ tlab_refill(retry_tlab, try_eden, slow_path); // preserves r19 & r3, returns rthread

                    __ bind(retry_tlab);

                    // get the allocation size: round_up(hdr + length << (layout_helper & 0x1F))
                    // since size is positive ldrw does right thing on 64bit
                    __ ldrw(t1, Address(klass, Klass::layout_helper_offset()));
                    __ lslvw(arr_size, length, t1);
                    __ ubfx(t1, t1, Klass::_lh_header_size_shift,
                            exact_log2(Klass::_lh_header_size_mask + 1));
                    __ add(arr_size, arr_size, t1);
                    __ add(arr_size, arr_size, MinObjAlignmentInBytesMask); // align up
                    __ andr(arr_size, arr_size, ~MinObjAlignmentInBytesMask);

                    __ tlab_allocate(obj, arr_size, 0, t1, t2, slow_path);  // preserves arr_size

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

                    __ bind(try_eden);
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
                    __ incr_allocated_bytes(thread, arr_size, 0, rscratch1);

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
                }

                __ enter();
                OopMap* map = save_live_registers(sasm);
                int call_offset;
                if (id == new_type_array_id) {
                    call_offset = __ call_RT(obj, noreg, CAST_FROM_FN_PTR(address, new_type_array), klass, length);
                } else {
                    call_offset = __ call_RT(obj, noreg, CAST_FROM_FN_PTR(address, new_object_array), klass, length);
                }

                oop_maps = new OopMapSet();
                oop_maps->add_gc_map(call_offset, map);
                restore_live_registers_except_r0(sasm);

                __ verify_oop(obj);
                __ leave();
                __ ret(lr);

                // r0: new array
            }
            break;

            case new_multi_array_id:
            { StubFrame f(sasm, "new_multi_array", dont_gc_arguments);
                // r0,: klass
                // r19,: rank
                // r2: address of 1st dimension
                OopMap* map = save_live_registers(sasm);
                __ mov(c_rarg1, r0);
                __ mov(c_rarg3, r2);
                __ mov(c_rarg2, r19);
                int call_offset = __ call_RT(r0, noreg, CAST_FROM_FN_PTR(address, new_multi_array), r1, r2, r3);

                oop_maps = new OopMapSet();
                oop_maps->add_gc_map(call_offset, map);
                restore_live_registers_except_r0(sasm);

                // r0,: new multi array
                __ verify_oop(r0);
            }
            break;

            case register_finalizer_id:
            {
                __ set_info("register_finalizer", dont_gc_arguments);

                // This is called via call_runtime so the arguments
                // will be place in C abi locations

                __ verify_oop(c_rarg0);

                // load the klass and check the has finalizer flag
                Label register_finalizer;
                Register t = r5;
                __ load_klass(t, r0);
                __ ldrw(t, Address(t, Klass::access_flags_offset()));
                __ tst(t, JVM_ACC_HAS_FINALIZER);
                __ br(Assembler::NE, register_finalizer);
                __ ret(lr);

                __ bind(register_finalizer);
                __ enter();
                OopMap* oop_map = save_live_registers(sasm);
                int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, SharedRuntime::register_finalizer), r0);
                oop_maps = new OopMapSet();
                oop_maps->add_gc_map(call_offset, oop_map);

                // Now restore all the live registers
                restore_live_registers(sasm);

                __ leave();
                __ ret(lr);
            }
            break;

            case throw_class_cast_exception_id:
            { StubFrame f(sasm, "throw_class_cast_exception", dont_gc_arguments);
                oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_class_cast_exception), true);
            }
            break;

            case throw_incompatible_class_change_error_id:
            { StubFrame f(sasm, "throw_incompatible_class_cast_exception", dont_gc_arguments);
                oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_incompatible_class_change_error), false);
            }
            break;

            case slow_subtype_check_id:
            {
                // Typical calling sequence:
                // __ push(klass_RInfo);  // object klass or other subclass
                // __ push(sup_k_RInfo);  // array element klass or other superclass
                // __ bl(slow_subtype_check);
                // Note that the subclass is pushed first, and is therefore deepest.
                enum layout {
                    r0_off, r0_off_hi,
                    r2_off, r2_off_hi,
                    r4_off, r4_off_hi,
                    r5_off, r5_off_hi,
                    sup_k_off, sup_k_off_hi,
                    klass_off, klass_off_hi,
                    framesize,
                    result_off = sup_k_off
                };

                __ set_info("slow_subtype_check", dont_gc_arguments);
                __ push(RegSet::of(r0, r2, r4, r5), sp);

                // This is called by pushing args and not with C abi
                // __ ldr(r4, Address(sp, (klass_off) * VMRegImpl::stack_slot_size)); // subclass
                // __ ldr(r0, Address(sp, (sup_k_off) * VMRegImpl::stack_slot_size)); // superclass

                __ ldp(r4, r0, Address(sp, (sup_k_off) * VMRegImpl::stack_slot_size));

                Label miss;
                __ check_klass_subtype_slow_path(r4, r0, r2, r5, NULL, &miss);

                // fallthrough on success:
                __ mov(rscratch1, 1);
                __ str(rscratch1, Address(sp, (result_off) * VMRegImpl::stack_slot_size)); // result
                __ pop(RegSet::of(r0, r2, r4, r5), sp);
                __ ret(lr);

                __ bind(miss);
                __ str(zr, Address(sp, (result_off) * VMRegImpl::stack_slot_size)); // result
                __ pop(RegSet::of(r0, r2, r4, r5), sp);
                __ ret(lr);
            }
            break;

            case monitorenter_nofpu_id:
                save_fpu_registers = false;
            // fall through
            case monitorenter_id:
            {
                StubFrame f(sasm, "monitorenter", dont_gc_arguments);
                OopMap* map = save_live_registers(sasm, save_fpu_registers);

                // Called with store_parameter and not C abi

                f.load_argument(1, r0); // r0,: object
                f.load_argument(0, r1); // r1,: lock address

                int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, monitorenter), r0, r1);

                oop_maps = new OopMapSet();
                oop_maps->add_gc_map(call_offset, map);
                restore_live_registers(sasm, save_fpu_registers);
            }
            break;

            case monitorexit_nofpu_id:
                save_fpu_registers = false;
            // fall through
            case monitorexit_id:
            {
                StubFrame f(sasm, "monitorexit", dont_gc_arguments);
                OopMap* map = save_live_registers(sasm, save_fpu_registers);

                // Called with store_parameter and not C abi

                f.load_argument(0, r0); // r0,: lock address

                // note: really a leaf routine but must setup last java sp
                //       => use call_RT for now (speed can be improved by
                //       doing last java sp setup manually)
                int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, monitorexit), r0);

                oop_maps = new OopMapSet();
                oop_maps->add_gc_map(call_offset, map);
                restore_live_registers(sasm, save_fpu_registers);
            }
            break;

            case deoptimize_id:
            {
                StubFrame f(sasm, "deoptimize", dont_gc_arguments);
                OopMap* oop_map = save_live_registers(sasm);
                int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, deoptimize));
                oop_maps = new OopMapSet();
                oop_maps->add_gc_map(call_offset, oop_map);
                restore_live_registers(sasm);
                DeoptimizationBlob* deopt_blob = SharedRuntime::deopt_blob();
                assert(deopt_blob != NULL, "deoptimization blob must have been created");
                __ leave();
                __ far_jump(RuntimeAddress(deopt_blob->unpack_with_reexecution()));
            }
            break;

            case throw_range_check_failed_id:
            { StubFrame f(sasm, "range_check_failed", dont_gc_arguments);
                oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_range_check_exception), true);
            }
            break;

            case unwind_exception_id:
            { __ set_info("unwind_exception", dont_gc_arguments);
                // note: no stubframe since we are about to leave the current
                //       activation and we are calling a leaf VM function only.
                generate_unwind_exception(sasm);
            }
            break;

            case access_field_patching_id:
            { StubFrame f(sasm, "access_field_patching", dont_gc_arguments);
                // we should set up register map
                oop_maps = generate_patching(sasm, CAST_FROM_FN_PTR(address, access_field_patching));
            }
            break;

            case load_klass_patching_id:
            { StubFrame f(sasm, "load_klass_patching", dont_gc_arguments);
                // we should set up register map
                oop_maps = generate_patching(sasm, CAST_FROM_FN_PTR(address, move_klass_patching));
            }
            break;

            case load_mirror_patching_id:
            { StubFrame f(sasm, "load_mirror_patching", dont_gc_arguments);
                // we should set up register map
                oop_maps = generate_patching(sasm, CAST_FROM_FN_PTR(address, move_mirror_patching));
            }
            break;

            case load_appendix_patching_id:
            { StubFrame f(sasm, "load_appendix_patching", dont_gc_arguments);
                // we should set up register map
                oop_maps = generate_patching(sasm, CAST_FROM_FN_PTR(address, move_appendix_patching));
            }
            break;

            case handle_exception_nofpu_id:
            case handle_exception_id:
            { StubFrame f(sasm, "handle_exception", dont_gc_arguments);
                oop_maps = generate_handle_exception(id, sasm);
            }
            break;

            case handle_exception_from_callee_id:
            { StubFrame f(sasm, "handle_exception_from_callee", dont_gc_arguments);
                oop_maps = generate_handle_exception(id, sasm);
            }
            break;

            case throw_index_exception_id:
            { StubFrame f(sasm, "index_range_check_failed", dont_gc_arguments);
                oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_index_exception), true);
            }
            break;

            case throw_array_store_exception_id:
            { StubFrame f(sasm, "throw_array_store_exception", dont_gc_arguments);
                // tos + 0: link
                //     + 1: return address
                oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_array_store_exception), true);
            }
            break;

#if INCLUDE_ALL_GCS

            case g1_pre_barrier_slow_id:
      {
        StubFrame f(sasm, "g1_pre_barrier", dont_gc_arguments);
        // arg0 : previous value of memory

        BarrierSet* bs = Universe::heap()->barrier_set();
        if (bs->kind() != BarrierSet::G1SATBCTLogging && bs->kind() != BarrierSet::ShenandoahBarrierSet) {
	  __ mov(r0, (int)id);
	  __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, unimplemented_entry), r0);
	  __ should_not_reach_here();
          break;
        }

        const Register pre_val = r0;
        const Register thread = rthread;
        const Register tmp = rscratch1;

        Address in_progress(thread, in_bytes(JavaThread::satb_mark_queue_offset() +
                                             PtrQueue::byte_offset_of_active()));

        Address queue_index(thread, in_bytes(JavaThread::satb_mark_queue_offset() +
                                             PtrQueue::byte_offset_of_index()));
        Address buffer(thread, in_bytes(JavaThread::satb_mark_queue_offset() +
                                        PtrQueue::byte_offset_of_buf()));

        Label done;
        Label runtime;

        // Can we store original value in the thread's buffer?
        __ ldr(tmp, queue_index);
        __ cbz(tmp, runtime);

        __ sub(tmp, tmp, wordSize);
        __ str(tmp, queue_index);
        __ ldr(rscratch2, buffer);
	__ add(tmp, tmp, rscratch2);
	f.load_argument(0, rscratch2);
        __ str(rscratch2, Address(tmp, 0));
        __ b(done);

        __ bind(runtime);
        __ push_call_clobbered_registers();
        f.load_argument(0, pre_val);
        __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::g1_wb_pre), pre_val, thread);
        __ pop_call_clobbered_registers();
        __ bind(done);
      }
      break;
    case g1_post_barrier_slow_id:
      {
        StubFrame f(sasm, "g1_post_barrier", dont_gc_arguments);

        // arg0: store_address
        Address store_addr(rfp, 2*BytesPerWord);

        BarrierSet* bs = Universe::heap()->barrier_set();
        if (bs->kind() == BarrierSet::ShenandoahBarrierSet) {
          __ movptr(r0, (int)id);
          __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, unimplemented_entry), r0);
          __ should_not_reach_here();
          break;
        }

        CardTableModRefBS* ct = (CardTableModRefBS*)bs;
        assert(sizeof(*ct->byte_map_base) == sizeof(jbyte), "adjust this code");

        Label done;
        Label runtime;

        // At this point we know new_value is non-NULL and the new_value crosses regions.
        // Must check to see if card is already dirty

        const Register thread = rthread;

        Address queue_index(thread, in_bytes(JavaThread::dirty_card_queue_offset() +
                                             PtrQueue::byte_offset_of_index()));
        Address buffer(thread, in_bytes(JavaThread::dirty_card_queue_offset() +
                                        PtrQueue::byte_offset_of_buf()));

        const Register card_offset = rscratch2;
        // LR is free here, so we can use it to hold the byte_map_base.
        const Register byte_map_base = lr;

        assert_different_registers(card_offset, byte_map_base, rscratch1);

        f.load_argument(0, card_offset);
        __ lsr(card_offset, card_offset, CardTableModRefBS::card_shift);
        __ load_byte_map_base(byte_map_base);
        __ ldrb(rscratch1, Address(byte_map_base, card_offset));
        __ cmpw(rscratch1, (int)G1SATBCardTableModRefBS::g1_young_card_val());
	__ br(Assembler::EQ, done);

	assert((int)CardTableModRefBS::dirty_card_val() == 0, "must be 0");

        __ membar(Assembler::StoreLoad);
        __ ldrb(rscratch1, Address(byte_map_base, card_offset));
	__ cbzw(rscratch1, done);

        // storing region crossing non-NULL, card is clean.
        // dirty card and log.
        __ strb(zr, Address(byte_map_base, card_offset));

        // Convert card offset into an address in card_addr
        Register card_addr = card_offset;
        __ add(card_addr, byte_map_base, card_addr);

        __ ldr(rscratch1, queue_index);
        __ cbz(rscratch1, runtime);
        __ sub(rscratch1, rscratch1, wordSize);
        __ str(rscratch1, queue_index);

        // Reuse LR to hold buffer_addr
        const Register buffer_addr = lr;

	__ ldr(buffer_addr, buffer);
	__ str(card_addr, Address(buffer_addr, rscratch1));
	__ b(done);

        __ bind(runtime);
        __ push_call_clobbered_registers();
        __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::g1_wb_post), card_addr, thread);
        __ pop_call_clobbered_registers();
        __ bind(done);

      }
      break;
#endif

            case predicate_failed_trap_id:
            {
                StubFrame f(sasm, "predicate_failed_trap", dont_gc_arguments);

                OopMap* map = save_live_registers(sasm);

                int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, predicate_failed_trap));
                oop_maps = new OopMapSet();
                oop_maps->add_gc_map(call_offset, map);
                restore_live_registers(sasm);
                __ leave();
                DeoptimizationBlob* deopt_blob = SharedRuntime::deopt_blob();
                assert(deopt_blob != NULL, "deoptimization blob must have been created");

                __ far_jump(RuntimeAddress(deopt_blob->unpack_with_reexecution()));
            }
            break;


            default:
            { StubFrame f(sasm, "unimplemented entry", dont_gc_arguments);
                __ mov(r0, (int)id);
                __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, unimplemented_entry), r0);
                __ should_not_reach_here();
            }
            break;
        }
    }
    return oop_maps;
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
}

void Runtime0::generate_patching(StubAssembler *sasm, address target) {
    // use the maximum number of runtime-arguments here because it is difficult to
    // distinguish each RT-Call.
    // Note: This number affects also the RT-Call in generate_handle_exception because
    //       the oop-map is shared for all calls.
    DeoptimizationBlob* deopt_blob = SharedRuntime::deopt_blob();
    assert(deopt_blob != NULL, "deoptimization blob must have been created");

    OopMap* oop_map = save_live_registers(sasm);

    __ mov(c_rarg0, rthread);
    Label retaddr;
    __ set_last_Java_frame(sp, rfp, retaddr, rscratch1);
    // do the call
    __ lea(rscratch1, RuntimeAddress(target));
    __ blr(rscratch1);
    __ bind(retaddr);
    OopMapSet* oop_maps = new OopMapSet();
    oop_maps->add_gc_map(__ offset(), oop_map);
    // verify callee-saved register
#ifdef ASSERT
    { Label L;
    __ get_thread(rscratch1);
    __ cmp(rthread, rscratch1);
    __ br(Assembler::EQ, L);
    __ stop("StubAssembler::call_RT: rthread not callee saved?");
    __ bind(L);
  }
#endif
    __ reset_last_Java_frame(true);
    __ maybe_isb();

    // check for pending exceptions
    { Label L;
        __ ldr(rscratch1, Address(rthread, Thread::pending_exception_offset()));
        __ cbz(rscratch1, L);
        // exception pending => remove activation and forward to exception handler

        { Label L1;
            __ cbnz(r0, L1);                                  // have we deoptimized?
            __ far_jump(RuntimeAddress(Runtime1::entry_for(Runtime1::forward_exception_id)));
            __ bind(L1);
        }

        // the deopt blob expects exceptions in the special fields of
        // JavaThread, so copy and clear pending exception.

        // load and clear pending exception
        __ ldr(r0, Address(rthread, Thread::pending_exception_offset()));
        __ str(zr, Address(rthread, Thread::pending_exception_offset()));

        // check that there is really a valid exception
        __ verify_not_null_oop(r0);

        // load throwing pc: this is the return address of the stub
        __ mov(r3, lr);

#ifdef ASSERT
        // check that fields in JavaThread for exception oop and issuing pc are empty
    Label oop_empty;
    __ ldr(rscratch1, Address(rthread, Thread::pending_exception_offset()));
    __ cbz(rscratch1, oop_empty);
    __ stop("exception oop must be empty");
    __ bind(oop_empty);

    Label pc_empty;
    __ ldr(rscratch1, Address(rthread, JavaThread::exception_pc_offset()));
    __ cbz(rscratch1, pc_empty);
    __ stop("exception pc must be empty");
    __ bind(pc_empty);
#endif

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


    // Runtime will return true if the nmethod has been deoptimized during
    // the patching process. In that case we must do a deopt reexecute instead.

    Label reexecuteEntry, cont;

    __ cbz(r0, cont);                                 // have we deoptimized?

    // Will reexecute. Proper return address is already on the stack we just restore
    // registers, pop all of our frame but the return address and jump to the deopt blob
    restore_live_registers(sasm);
    __ leave();
    __ far_jump(RuntimeAddress(deopt_blob->unpack_with_reexecution()));

    __ bind(cont);
    restore_live_registers(sasm);
    __ leave();
    __ ret(lr);

    return oop_maps;

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

    if (TracePatching) {
        tty->print_cr("Deoptimizing because patch is needed");
    }

    frame runtime_frame = thread->last_frame();
    frame caller_frame = runtime_frame.sender(&reg_map);

    // It's possible the nmethod was invalidated in the last
    // safepoint, but if it's still alive then make it not_entrant.
    nmethod* nm = CodeCache::find_nmethod(caller_frame.pc());
    if (nm != NULL) {
        nm->make_not_entrant();
    }

    //Deoptimization::deoptimize_frame(thread, caller_frame.id());

    // Return to the now deoptimized frame.
}

int Runtime0::compile_method_patching(JavaThread *thread) {
    patch_code(thread, compile_method_patching_id);
}

void Runtime0::gc_barrier(JavaThread *thread) {
    EnclaveGC::atomic_spin_gc();
}
