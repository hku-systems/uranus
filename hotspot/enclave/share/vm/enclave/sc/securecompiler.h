
#ifndef ENCLAVE_VM_OPTO_C2SECURECOMPILER_HPP
#define ENCLAVE_VM_OPTO_C2SECURECOMPILER_HPP


//#include "compiler/abstractCompiler.hpp"

//AbstractCompiler* c1compiler;
#include <sgx_trts_exception.h>

#if defined(__cplusplus)
extern "C" {
#endif

int enclave_signal_handler(sgx_exception_info_t *info);

void* c1_initialize(void*, void**, void**, void** klass_list, int debug);
int within_enclave(void *addr);
//void* interpreter_wrapper(void* rbx_buf, );
void* interpreted_entry_zero_locals(void *, void*, int*);
void gc_scavenge(void *queue, int n);
#if defined(__cplusplus)
}
#endif



//void c1_compile_method(ciEnv* env,
//            ciMethod* target,
//            int entry_bci);

#endif // ENCLAVE_VM_OPTO_C2SECURECOMPILER_HPP
