
#include <precompiled.hpp>
#include <sgx_thread.h>
#include <sgx_trts_exception.h>
#include "classfile/symbolTable.hpp"
#include "EnclaveNative.h"
#include "EnclaveOcall.h"
#include "EnclaveException.h"
#include "EnclaveMemory.h"

#include "sgx_trts.h"
#include "EnclaveRuntime.h"

// frame layout of calling into enclave

// Arguments:
//
// rbx: Method*
//
// Stack layout immediately at entry
//

//    [expression stack      ] * <- sp
//    [monitors              ]   \
//     ...                        | monitor block size
//    [monitors              ]   /
//    [monitor block size    ]
//    [byte code index/pointr]                   = bcx()                bcx_offset
//    [pointer to locals     ]                   = locals()             locals_offset
//    [constant pool cache   ]                   = cache()              cache_offset
//    [methodData            ]                   = mdp()                mdx_offset
//    [Method*               ]                   = method()             method_offset
//    [last sp               ]                   = last_sp()            last_sp_offset
//    [old stack pointer     ]                     (sender_sp)          sender_sp_offset
//    [old frame pointer     ]   <- fp           = link()
//    [ return address     ] <--- rsp when calling into enclave
//    [ parameter n        ]
//      ...
//    [ parameter 1        ]
//    [ expression stack   ] (caller's java expression stack)


// Stack frame when an ecall happend
//
//    [ ret_addr ] -> enclave_interpreter_ret
//    [ 0 ]
//    [ 0 ]
//    [ expression stack   ] (ecall rsp) <- r14

// register used across encalve
// rbx [ Method* ]]
// r13 -> clear
// r14 -> old parameter top
// r15 -> [ JavaThread* ]
// r11 [ 0: normal call, 1 -> ecall ]



void* c1_initialize(void* cpuid, void** heap_top, void** heap_bottom, void** klass_list, int debug) {
    return EnclaveRuntime::init(cpuid, heap_top, heap_bottom, klass_list, debug);
}


void* interpreted_entry_zero_locals(void *rbx_buf, void* m, int* has_exception) {
    return EnclaveRuntime::do_ecall_comp(rbx_buf, m, has_exception);
}

int within_enclave(void *addr) {
    if (sgx_is_within_enclave(addr, 1)) {
        oop e_obj = (oop)addr;
        if (e_obj->is_oop(false))
            printf("enclave object is %s\n", e_obj->klass()->external_name());
        return true;
    }
    return false;
}

void gc_scavenge(void *queue, int n) {
    EnclaveGC::gc_start((StarTask*)queue, n);
}

//void c1_compile_method(ciEnv* env,
//        ciMethod* target,
//        int entry_bci) {
//          c1compiler->compile_method(env, target, entry_bci);
//        }
