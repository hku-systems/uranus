//
// Created by max on 3/21/18.
//

#ifndef HOTSPOT_ENCLAVEDEBUG_H
#define HOTSPOT_ENCLAVEDEBUG_H

#ifdef ENCLAVE_UNIX
// print every frame information
//#define DB_FRAME
//#define DB_GC
//#define DB_MEM
#define DB_EXP
#define DB_WARN
#define DB_COMP
//#define DB_GRAPH
//#define DB_LIR_CODE
//#define DB_OCALL

#define INIT_MPX_BOUND     asm("lea %0, %%rax;" \
                                "bndmov (%%rax), %%bnd0;" \
                                : \
                                :"m"(EnclaveRuntime::bounds));

#ifdef ENCLAVE_MPX
#define ENABLE_MPX() { enable_mpx(); INIT_MPX_BOUND; }
#else
#define ENABLE_MPX()
#endif

void error_backtrace_fp(void* fp, void* sp, void* pc);

void print_enclave_frame(void* , void* sender_sp, void* method);

void exit_enclave_frame(void* th, void* m);

void print_native_enclave_frame(void *t, void* addr, void* m);

void print_exception_frame(void* thread, void* exp_oop);

#endif

#ifdef DB_FRAME
#define FRAME_DEBUG(...) printf(__VA_ARGS__)
#else
#define FRAME_DEBUG(...)
#endif

#define COLOR_RED       "[0;31m"
#define COLOR_GREEN     "[0;32m"
#define COLOR_YELLOW    "[0;33m"
#define COLOR_BLUE      "[0;34m"


#define P_COLOR(color, text) "\033"color text"\033[0m"

#define D_INFO(text) P_COLOR(COLOR_GREEN, "["text"]")
#define D_WARN(text) P_COLOR(COLOR_YELLOW, "["text"]")
#define D_ERROR(text) P_COLOR(COLOR_RED, "["text"]")
#define D_NORMAL(text) P_COLOR(COLOR_BLUE, "["text"]")

#ifdef ENCLAVE_UNIX
#ifdef DB_WARN
//#define D_WARN_Unstable printf(D_WARN("Unstable")" using unstable function call: %s:%s:%d\n", __FUNCTION__, __FILE__, __LINE__)
#define D_WARN_Unstable
//#define D_WARN_Unimplement printf(D_WARN("Un-implemented")" using un-implemented function call: %s:%s:%d\n", __FUNCTION__, __FILE__, __LINE__)
#define D_WARN_Unimplement
#define D_FIX
//#define D_FIX printf(D_WARN("TODO")" Fix: %s:%s:%d\n", __FUNCTION__, __FILE__, __LINE__)
// #define Unimplemented(x) printf(D_WARN("Un-implemented")" using un-implemented function call: %s:%s:%d\n", __FUNCTION__, __FILE__, __LINE__)
#define D_COMPILE(x)
#define Unimplemented(x)
#else
#define D_WARN_Unstable
#define D_WARN_Unimplement
#define D_COMPILE(x)
#define Unimplemented(x)
#endif
#else
#define D_WARN_Unstable
#define D_WARN_Unimplement
#endif

#define CallerTraceDefs , const char* t_func = __builtin_FUNCTION(), const char* t_file = __builtin_FILE(), int t_line = __builtin_LINE()
#define CallerTraceArgs , const char* t_func, const char* t_file, int t_line

#ifdef DB_OCALL
#define OCallCallerArgs CallerTraceArgs
#define OCallCallerDefs CallerTraceDefs
#define OCallTrace printf(D_INFO("DO_OCALL")" Do Ocall: %s <- %s:%s:%d\n", __FUNCTION__, t_func, t_file, t_line);
#else
#define OCallCallerArgs
#define OCallCallerDefs
#define OCallTrace
#endif

#define TraceMem printf(D_INFO("MemTrace")"allocate mem in %s -> %s:%d", t_func, t_file, t_line)

#endif //HOTSPOT_ENCLAVEDEBUG_H
