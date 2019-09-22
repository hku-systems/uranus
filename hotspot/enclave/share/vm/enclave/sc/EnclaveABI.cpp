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
        BufferBlob* stub_blob = BufferBlob::create("EnclaveABI:", 512);
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
#ifdef __x86_64__
#   define __ _masm->
            address ret = __ pc();
            interpreter_ret = ret;
            // regular exit path
            __ movptr(r11, 0);
            __ movptr(rsp, rbp);
            __ pop(rbp);
            __ ret(0);
            exception_ret = __ pc();
            __ movptr(r11, 1);
            __ movptr(rsp, rbp);
            __ pop(rbp);
            __ ret(0);
            address start = __ pc();
            // restore the sp and bp first, then restore the rbx and r14
            // __ movptr(rbp, Address(c_rarg0, 0));
            __ push(rbp);
            __ mov(rbp, rsp);
            __ movptr(rax, (intptr_t)ret);
            __ movptr(r12, c_rarg1);
            __ movptr(Address(r12, 0), rsp);

            __ movptr(rsp, c_rarg0);

            __ pop(r15);
            Address non_enclave_thread(c_rarg2, Thread::normal_thread_offset());
            __ movptr(non_enclave_thread, r15);
            __ mov(r15, c_rarg2);
            __ pop(r14);
            __ pop(r13);
            __ pop(r11);
            __ pop(r10);
            __ pop(rsi);
            __ pop(rdi);
            __ pop(rdx);
            __ pop(rcx);
            __ pop(rbx);

            // clear r13, so the sender sp is 0, used for gc
            __ movptr(r13, rsp);

            // get the ret address, it may cause SEV if a pop/push is used
//      __ movptr(rax, Address(rsp, 0));
            // we have add three stack element, so we need to pack it
            // why 5 instead of 4 ???
            __ lea(r14, Address(rsp, 12, Address::times_8, wordSize));
            __ movptr(Address(rsp, 0), 0);
            __ movptr(Address(rsp, 4), 0);
            __ movptr(rsp, Address(r12, 0));

            // push the ret address
        //     __ push(rax);

            __ movptr(r11, Address(rbx, Method::kind_offset()));
            // __ movptr(r12, (intptr_t)AbstractInterpreter::getEntryTable());
            __ movptr(r12, Address(rbx, Method::enclave_native_function_offset()));
            // tell that this is a ecall to the interpreter
        //     __ movl(r11, 1);
        //     __ jmp(r12);
             __ call(r12);
             // regular exit path
             __ movptr(r11, 0);
             __ movptr(rsp, rbp);
             __ pop(rbp);
             __ ret(0);
            address get_exception_addr = __ pc();
            __ mov(rax, r11);
            __ ret(0);
            get_exception = CAST_TO_FN_PTR(get_exception_stub_t , get_exception_addr);
        return start;
#endif
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
