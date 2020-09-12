//
// Created by jianyu on 6/6/18.
//

#include <precompiled.hpp>
#include "EnclaveABI.h"
#include "EnclaveMemory.h"
#include "EnclaveOcall.h"
// TODO: frame info of ocall

address EnclaveABI::do_ocall = NULL;
address EnclaveABI::exception_ret = NULL;
address EnclaveABI::interpreter_ret = NULL;
interpreter_stub_t EnclaveABI::do_ecall = NULL;
get_exception_stub_t EnclaveABI::get_exception = NULL;
BufferBlob* EnclaveABI::stub_blob = NULL;

void* EnclaveABI::copy_parameter(void *r14, int size) {
        intptr_t* heap_rsp = (intptr_t*)EnclaveMemory::heapMemory->alloc(size, NULL);
        intptr_t* start = (intptr_t*)r14;
        // reverse the stack and copy it to the heap
        for (int i = 0;i < size;i++) {
                heap_rsp[i] = start[-i];
        }
        return heap_rsp;
}

void* EnclaveABI::call_interpreter(void *r14, int size, void *method, void *thread, void* sender) {
        char* ret;
        EnclaveRuntime::ocall_count += 1;
        ocall_interpreter((void**)&ret, r14, size, method, thread, sender);
        return ret;
}


void EnclaveABI::init() {
        ResourceMark rm;
        stub_blob = BufferBlob::create("EnclaveABI:", 512);
        if (stub_blob == NULL) {
                ShouldNotReachHere();
        }
        CodeBuffer c(stub_blob);
        EnclaveABI g = EnclaveABI(&c);
        do_ecall = CAST_TO_FN_PTR(interpreter_stub_t, g.generate_interpreter_entry());
        do_ocall = g.generate_ocall_entry();
        EnclaveMemory::fast_heap_alloc = CAST_TO_FN_PTR(heap_allocator, g.generate_heap_alloc());

}

    EnclaveABI::EnclaveABI(CodeBuffer *c) : StubCodeGenerator(c) {}

    address EnclaveABI::generate_interpreter_entry() {
        StubCodeMark mark(this, "ENCLAVE_ABI", "interpreter_stub");
        address ret, start;
        #define __ _masm->
        ret = __ pc();
        interpreter_ret = ret;

        // regular exit path
        __ leave();
        __ ret(lr);

        start = __ pc();
        __ enter();

        // __ mov(sp, c_rarg0);
        __ mov(rmethod, c_rarg3);
        __ mov(rthread, c_rarg2);

        int words_pushed = 0;

        // Scan bitset to accumulate register pairs
        unsigned char regs[32];
        int count = 0;
        for (int reg = 0; reg <= 30; reg++) {
            if (reg == rfp->encoding()
                || reg == lr->encoding()
                || reg == rthread->encoding()
                || reg == rmethod->encoding()
                || reg == esp->encoding()) {
                regs[count++] = reg % 2 + 1;
            } else
                regs[count++] = reg;
        }
        regs[count++] = zr->encoding_nocheck();
        count &= ~1;

        for (int i = 2; i < count; i += 2) {
            __ ldp(as_Register(regs[i]), as_Register(regs[i+1]),
                Address(c_rarg0, i * wordSize));
            words_pushed += 2;
        }

        // __ ldp(as_Register(regs[0]), as_Register(regs[1]),
        //     Address(__ post(c_rarg0, count * wordSize)));
        words_pushed += 2;

        __ lea(rlocals, Address(c_rarg0, (0x20 + 0x20 + 5) * wordSize));

        __ sub(esp, sp, 40 * wordSize);

        __ mov(r13, sp);
        __ ldr(rscratch1, Address(rmethod, Method::from_compiled_offset()));
        __ blr(rscratch1);
        __ leave();
        __ ret(lr);

        return start;
        #undef __
    };

    address EnclaveABI::generate_heap_alloc() {
#ifdef __x86_64__
            address new_heap_object = __ pc();

            const Register RtopAddr = rdi;
            const Register RendAddr = rsi;
            const Register Rsize    = rdx;

            __ push(rbx);
            __ push(rbp);
            __ mov(rbp, rsp);
            __ movptr(rax, Address(RtopAddr, 0));

            // For retries rax gets set by cmpxchgq
            Label retry, slow_case, fin;
            __ bind(retry);
            __ lea(rbx, Address(rax, Rsize, Address::times_1));
            __ cmpptr(rbx, Address(RendAddr, 0));
            __ jcc(Assembler::above, slow_case);

            // Compare rax with the top addr, and if still equal, store the new
            // top addr in rbx at the address of the top addr pointer. Sets ZF if was
            // equal, and clears it otherwise. Use lock prefix for atomicity on MPs.
            //
            // rax: object begin
            // rbx: object end
            // rdx: instance size in bytes
            if (os::is_MP()) {
                    __ lock();
            }
            __ cmpxchgptr(rbx, Address(RtopAddr, 0));

            // if someone beat us on the allocation, try again, otherwise continue
            __ jcc(Assembler::notEqual, retry);

            // should put r15 here first
            // __ incr_allocated_bytes(r15_thread, Rsize, 0);

            __ jmp(fin);
            __ bind(slow_case);
            __ movptr(rax, 0);
            __ bind(fin);
            __ mov(rsp, rbp);
            __ pop(rbp);
            __ pop(rbx);
            __ ret(0);
        #   undef __
            return new_heap_object;
#endif
    }

    address EnclaveABI::generate_ocall_entry() {
#ifdef __x86_64__
#   define __ _masm->
        address entry = __ pc();
        // first we get the number of parameters
        const Address size_of_parameters(rdx,
                                         ConstMethod::size_of_parameters_offset());
        const Address constMethod(rbx, Method::const_offset());
        __ movptr(rdx, constMethod);
        __ load_unsigned_short(rcx, size_of_parameters);
        __ lea(r14, Address(rsp, rcx, Address::times_8, -wordSize));
        __ addptr(r14, 8);
        __ mov(c_rarg0, r14);
        __ mov(c_rarg1, rcx);
        __ push(rcx);
        __ movptr(r12, (intptr_t)EnclaveABI::copy_parameter);
        __ call(r12);

        // ocall parameter: parameter_start, rbx, r15
        __ pop(rcx);
        __ mov(c_rarg0, rax);
        __ mov(c_rarg1, rcx);
        __ mov(c_rarg2, rbx);
        Address non_enclave_thread(r15_thread, Thread::normal_thread_offset());
        __ movptr(c_rarg3, non_enclave_thread);
        __ mov(c_rarg4, r13);
        __ movptr(r12, (intptr_t)EnclaveABI::call_interpreter);
        __ call(r12);

        // get ret addr and jmp
        __ ret(0);
        return entry;
#   undef __
#endif
    }
