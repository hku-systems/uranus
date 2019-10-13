//
// Created by max on 12/29/17.
//

#ifndef HOTSPOT_COMPILERENCLAVE_H
#define HOTSPOT_COMPILERENCLAVE_H


#include <oops/method.hpp>
#include <utilities/taskqueue.hpp>
#include "Enclave.h"
#include "CompilerEnclave.h"
#include "FastList.h"

typedef unsigned char u_char;

#define ENCLAVE_SO_FILE "libjvm-sgx.so"

class CompilerEnclave : Enclave {
public:

    static char* enclave_so_path;

    CompilerEnclave();

    // define the function here
    void compiler_initialize();

    static void compiler_gc(FastList<StarTask> *list);

    int in_enclave(void *addr);

    void* interpreter_entry_zero_locals(void*, void*, int* );

    static void init();

    static void* call_interpreter_zero_locals(void*, void *, void*);

    static bool in_enclave_heap(intptr_t addr);

    static CompilerEnclave* shareEnclave;

    static intptr_t enclave_heap_start;

    static intptr_t enclave_heap_end;
};


#endif //HOTSPOT_COMPILERENCLAVE_H
