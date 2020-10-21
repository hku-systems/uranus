//
// Created by max on 12/29/17.
//

#include "precompiled.hpp"
#include "CompilerEnclave.h"
#include "EnclaveManager.h"
#include "securecompiler_u.h"
// #include "enclave/sc/EnclaveNative.h"

char* CompilerEnclave::enclave_so_path = NULL;

CompilerEnclave* CompilerEnclave::shareEnclave = NULL;

intptr_t CompilerEnclave::enclave_heap_start = 0;

intptr_t CompilerEnclave::enclave_heap_end = 0;

typedef void (*throw_exception_t)();

static throw_exception_t throw_now = NULL;

bool CompilerEnclave::in_enclave_heap(intptr_t addr) {
    return (addr >= enclave_heap_start && addr <= enclave_heap_end && addr);
}

void CompilerEnclave::compiler_initialize() {
    char* ret_val;
    sgx_status_t ret = c1_initialize(id, (void**)&ret_val,
                                                    VM_Version::get_cpuid_info(),
                                                    (void**)Universe::heap()->top_addr(),
                                                    (void**)Universe::heap()->end_addr(),
                                                    (void**)SystemDictionary::well_known_klass_addr((SystemDictionary::WKID)(0)), 0);
    enclave_heap_start = (intptr_t)ret_val;
    enclave_heap_end = enclave_heap_start + 10240;
}

int CompilerEnclave::in_enclave(void *addr) {
    int retval = 0;
    within_enclave(id, &retval, addr);
    return retval;
}

CompilerEnclave::CompilerEnclave(){
    id = EnclaveManager::new_enclave("/usr/lib/libjvm-enclave.so");
//    printf("enclave path: %s\n", enclave_so_path);
}

void* CompilerEnclave::interpreter_entry_zero_locals(void *rsp_buf, void* method, int* has_exception) {
  u_char *ret_buf;
//  printf(D_INFO("ENCLAVE")" Enter Enclave\n");
  interpreted_entry_zero_locals(id, (void**)&ret_buf, rsp_buf, method, has_exception);
  // BREAKPOINT;
  if (*has_exception) {
    // printf("has exception\n");
    JavaThread* THREAD = JavaThread::current();
    CLEAR_PENDING_EXCEPTION;
    THROW_MSG_0(vmSymbols::java_lang_RuntimeException(), "Exception throw inside enclave");
  }
//  printf(D_INFO("ENCLAVE")" Exit Enclave\n");
  return ret_buf;
}

void* CompilerEnclave::call_interpreter_zero_locals(void *rsp_buf, void *ret_addr, void* m) {
    // printf("call enclave\n");
    int has_exception = 0;

    u_char* r = (u_char*)shareEnclave->interpreter_entry_zero_locals(rsp_buf, m, &has_exception);
    // u_int64_t *rp_buf = (u_int64_t*)((u_char*)(ret_addr) + 8);
    // *rp_buf = (u_int64_t)r;
    if (has_exception) {
      // u_char* r = (u_char*)&TemplateInterpreter::_throw_forward_entry;
      return r;
    }
    return (u_char*)r;
}

void CompilerEnclave::compiler_gc(FastList<StarTask> *list) {
    // do not gc inside enclave
    // securecompiler_gc_scavenge(shareEnclave->id, list->arr, list->count);
}

void CompilerEnclave::init() {
    shareEnclave = new CompilerEnclave();
    shareEnclave->compiler_initialize();
}

void CompilerEnclave::get_jar_hash(char *buf) {

}
