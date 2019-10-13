//
// Created by max on 1/2/18.
//

#ifdef ENCLAVE_UNIX_USELESS
#include <precompiled.hpp>
#include "HeaderHelper.h"
#include "../opto/indexSet.hpp"
#include <stdio.h>

extern void syscall_invoke_count(const char* ss);
#include "../opto/c2_globals.hpp"
C2_FLAGS(MATERIALIZE_DEVELOPER_FLAG, MATERIALIZE_PD_DEVELOPER_FLAG, MATERIALIZE_PRODUCT_FLAG, MATERIALIZE_PD_PRODUCT_FLAG, MATERIALIZE_DIAGNOSTIC_FLAG, MATERIALIZE_EXPERIMENTAL_FLAG, MATERIALIZE_NOTPRODUCT_FLAG)
#include <opto/c2compiler.hpp>
address OptoRuntime::handle_exception_C(JavaThread* thread) { syscall_invoke_count(__func__); }

void OptoRuntime::print_named_counters() { syscall_invoke_count(__func__); }

//bool OptoRuntime::is_callee_saved_register(MachRegisterNumbers reg) {}

//void IdealGraphPrinter::clean_up() { }

ExceptionBlob* OptoRuntime::_exception_blob;

void Compile::dump_inline_data(outputStream* out) { }

//void IndexSet::print_statistics() { }
//
//void Phase::print_timers() { }
//
//int OptoRuntime::ZapDeadCompiledLocals_count = 0;
//
//int Compile::_CompiledZap_count = 0;
//
//void Compile::print_statistics() { }

OptoReg::Name Matcher::interpreter_frame_pointer_reg() { return OptoReg::Name(RBP_num); }

// a very long string to avoid error
const unsigned char Matcher::_regEncode[10000];


bool C2Compiler::init_c2_runtime() { syscall_invoke_count(__func__); }

void C2Compiler::initialize() { syscall_invoke_count(__func__); }

// Compilation entry point for methods
void C2Compiler::compile_method(ciEnv* env,
                    ciMethod* target,
                    int entry_bci) { syscall_invoke_count(__func__); }

// sentinel value used to trigger backtracking in compile_method().
static const char* C2Compiler::retry_no_subsuming_loads() { syscall_invoke_count(__func__); }
static const char* C2Compiler::retry_no_escape_analysis() { syscall_invoke_count(__func__); }
static const char* C2Compiler::retry_class_loading_during_parsing() { syscall_invoke_count(__func__); }

// Print compilation timers and statistics
void C2Compiler::print_timers() { syscall_invoke_count(__func__); }

void debug_enclave(const char* c) {
    printf("%s\n", c);
}

#endif
