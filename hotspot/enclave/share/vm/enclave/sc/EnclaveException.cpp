//
// Created by Maxxie Jiang on 15/3/2018.
//

#include "EnclaveException.h"
#include "EnclaveOcall.h"
#include "enclave/sc/EnclaveMemory.h"
#include <precompiled.hpp>

char* EnclaveException::name_list[] = {0};
void* EnclaveException::entry_list[] = {0};

void EnclaveException::init() {
#define VM_SYMBOL_ENUM(name, string) name_list[EXCEPTION_EUNM(name)]=string;
    DO_EXCEPTIONS(VM_SYMBOL_ENUM)
#undef VM_SYMBOL_ENUM

    for (int i = NO_EXP + 1;i < EXCEPTION_COUNT;i++) {
        entry_list[i] = KLASS_resolve_or_fail(name_list[i]);
    }
}

void EnclaveException::throw_msg(JavaThread* thread, Exceptions e, char* file, int line) {
  // rethrow exception if it is pending
  EXP_DEBUG("[Exception] Throw exception inside enclave %s %s %d\n", name_list[e], file, line);
  if (!thread->has_pending_exception()) {
    Klass* klass = get_exception_klass(e);
    if (e == EnclaveException::java_lang_OutOfMemoryError) {
        // reset the gc so that we can throw the error
        EnclaveGC::reset();
    }
    oop exp_oop = (oop)EnclaveMemory::static_vm_new_obj(thread, klass);
    thread->set_pending_exception(exp_oop, file, line);
  }
}

oop EnclaveException::get_exception_oop(JavaThread* THREAD, const char *name) {
    if (name == NULL) return NULL;
    EXP_DEBUG("[Exception] allocate exception oop %s\n", name);
    Klass* klass = NULL;
    for (int i = NO_EXP + 1; i < EXCEPTION_COUNT;i++) {
        if (strcmp(name, name_list[i]) == 0) {
            klass = (Klass*)entry_list[i];
        }
    }
    oop r = NULL;
    if (klass != NULL) {
        r = (oop)EnclaveMemory::static_vm_new_obj(THREAD, klass);
    }
    return r;
}

address EnclaveException::check_exit_addr(address ret_addr, JavaThread* thread) {
  EXP_DEBUG("[Exception] can not find exception handler, ret\n");
  if (ret_addr == EnclaveABI::interpreter_ret) {
    return 0;
  }
  return Interpreter::rethrow_exception_entry();
}
