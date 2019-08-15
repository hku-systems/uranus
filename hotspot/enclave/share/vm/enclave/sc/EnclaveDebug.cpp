//
// Created by max on 3/27/18.
//
#include <precompiled.hpp>
#include "EnclaveDebug.h"
#include "securecompiler.h"

int enclave_frame_count = 0;

void error_backtrace_fp(void* fp, void* sp, void* pc) {
    JavaThread* thread = JavaThread::current();
    frame f = frame((intptr_t*)sp, (intptr_t*)fp, (address)pc);
    Method* method;
    RegisterMap reg_map(thread, false);
    while(within_enclave(f.unextended_sp())) {
        method = f.interpreter_frame_method();
        printf("frame %s:%s:%d\n", method->klass_name()->as_C_string(), method->name()->as_C_string(), f.interpreter_frame_bcx());
        f = f.sender_for_interpreter_frame(&reg_map);
    }
}

void print_enclave_frame(void *t, void* addr, void* m) {
    enclave_frame_count += 1;
    char space_buf[enclave_frame_count + 1];
    for (int i = 0;i < enclave_frame_count;i++) {
      space_buf[i] = (char)32;
    }
    space_buf[enclave_frame_count] = 0;
    JavaThread* THREAD = (JavaThread*)t;
    address sender_sp = (address)addr;
    Method* method = (Method*)m;
    if (!within_enclave(sender_sp)) {
        printf(D_INFO("Frame")" Init frame into enclave %s\n", method->name()->as_C_string());
    } else {
        Method* call_method = THREAD->last_frame().interpreter_frame_method();
        int bci = THREAD->last_frame().interpreter_frame_bci();
        char* klass_caller = call_method->klass_name()->as_C_string();
        char* klass_callee = method->klass_name()->as_C_string();
        char* call_method_name = call_method->name()->as_C_string();
        char* callee_name = method->name()->as_C_string();
        // callee name cause sigsegv
        printf("%s"D_INFO("FRAME")" %s:%s:%d -> %s:%s\n", space_buf,
               klass_caller, call_method_name, bci,
               klass_callee, callee_name);
        resource_free_bytes(klass_callee,strlen(klass_caller));
        resource_free_bytes(klass_callee, strlen(klass_callee));
        resource_free_bytes(call_method_name, strlen(call_method_name));
        resource_free_bytes(callee_name, strlen(callee_name));
    }
}

void exit_enclave_frame(void* th, void* me) {
  Method* m = (Method*)me;
  char space_buf[enclave_frame_count + 1];
  for (int i = 0;i < enclave_frame_count;i++) {
    space_buf[i] = (char)32;
  }
  space_buf[enclave_frame_count] = 0;
  enclave_frame_count -= 1;
  char* klass_name = m->klass_name()->as_C_string();
  char* method_name = m->name()->as_C_string();
  printf("%s"D_INFO("FRAME")" ret %s:%s\n", space_buf,
         klass_name,
         method_name);
  resource_free_bytes(klass_name, strlen(klass_name));
  resource_free_bytes(method_name, strlen(method_name));
}

void print_native_enclave_frame(void *t, void* addr, void* m) {
    JavaThread* THREAD = (JavaThread*)t;
    address sender_sp = (address)addr;
    Method* method = (Method*)m;
    if (!within_enclave(sender_sp)) {
        printf(D_INFO("Frame")" Init frame into enclave %s\n", method->name()->as_C_string());
    } else {
        Method* call_method = THREAD->last_frame().interpreter_frame_method();
        int bci = THREAD->last_frame().interpreter_frame_bci();
        const char* klass_caller = (call_method->klass_name()) ? call_method->klass_name()->as_C_string() : "";
        const char* klass_callee = (method->klass_name()) ? method->klass_name()->as_C_string() : "";
        // callee name cause sigsegv
        printf(D_INFO("Native FRAME")" %s:%s:%d -> %s:%s\n",
               klass_caller, call_method->name()->as_C_string(), bci,
               klass_callee, method->name()->as_C_string());
    }
}

void print_exception_frame(void* t, void* exp) {
    JavaThread* thread = (JavaThread*)t;
    oop exp_oop = (oop)exp;
    Method* method = thread->last_frame().interpreter_frame_method();
    int bci = thread->last_frame().interpreter_frame_bci();
    printf(D_INFO("EXCEPTION")" method %s:%s:%d\n   [EXP] handle exception %s:%s:%d\n",
           method->klass_name()->as_C_string(), method->name()->as_C_string(), bci,
           exp_oop->klass()->name()->as_C_string(),
           thread->exception_file(), thread->exception_line());
}
