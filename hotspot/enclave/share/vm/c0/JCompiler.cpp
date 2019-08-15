//
// Created by jianyu on 11/11/18.
//

#include "precompiled.hpp"
#include "JCompiler.hpp"
#include "NormalCompileTask.h"
#include "NativeCompileTask.hpp"
#include "OCallCompileTask.hpp"
#include <enclave/sc/EnclaveNative.h>

JCompiler* JCompiler::compiler = NULL;

void JCompiler::compile_method(Method *method) {
    CompileTask *task = NULL;
    if (is_compile(method)) {
        // has been compile
        return;
    }
    int size = 0;
    if (method->method_kind() == AbstractInterpreter::sgx_ocall_entry) {
        task = new OCallCompileTask(method);
    } else if (!method->is_native()) {
        task = new NormalCompileTask(method);
        if (((NormalCompileTask*)task)->fast_compile()) {
            task->set_compile_entry();
            delete task;
            return;
        }
        size = task->compile(0);
        delete task;
        task = new NormalCompileTask(method);

    } else {
        task = new NativeCompileTask(method);
    }

    if (method->is_abstract()) {
        printf("compiling abstract method %s\n", method->name()->as_C_string());
        return;
    }

    task->compile(size);
    task->set_compile_entry();
//    if (task->method_entry != EnclaveABI::do_ocall)
//        task->print_disassembly();
}

bool JCompiler::setup_code_buffer(CodeBuffer *code, int call_stub_estimate) {
    int locs_buffer_size = 20 * (relocInfo::length_limit + sizeof(relocInfo));
    char* locs_buffer = NEW_RESOURCE_ARRAY(char, locs_buffer_size);
    code->insts()->initialize_shared_locs((relocInfo*)locs_buffer,
                                          locs_buffer_size / sizeof(relocInfo));
    code->initialize_consts_size(desired_max_constant_size());
    // Call stubs + two deopt handlers (regular and MH) + exception handler
    int stub_size = (call_stub_estimate * call_stub_size) +
                    exception_handler_size +
                    (2 * deopt_handler_size);
    if (stub_size >= code->insts_capacity()) return false;
    code->initialize_stubs_size(stub_size);
    return true;
}

bool JCompiler::is_compile(Method *method) {
    address method_entry = *method->enclave_native_function_addr();
    return sgx_is_within_enclave(method_entry, 1);
}
