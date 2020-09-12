//
// Created by jianyu on 6/6/18.
//

#include <precompiled.hpp>
#include <classfile/symbolTable.hpp>
#include <c0/c0_Runtime.hpp>
#include <libmpx/mpxrt.h>
#include "EnclaveRuntime.h"
#include "EnclaveABI.h"
#include "EnclaveNative.h"

extern void icache_init();
extern void VM_Version_init();
extern void chunkpool_init();
#ifdef ENCLAVE_BOUND_CHECK
#ifdef ENCLAVE_MPX
uintptr_t EnclaveRuntime::bounds[2] = { (uintptr_t)get_enclave_heap(), ~((uintptr_t)((char*)get_enclave_heap() + get_enclave_heap_size())) };
#endif
#endif
sgx_spinlock_t EnclaveRuntime::count_mutex = SGX_SPINLOCK_INITIALIZER;
volatile int EnclaveRuntime::in_enclave_thread_count = 0;
int EnclaveRuntime::debug_bit = 0;
int EnclaveRuntime::ecall_count = 0;
int EnclaveRuntime::ocall_count = 0;
bool EnclaveRuntime::is_init = false;
std::map<std::string, Klass*>* EnclaveRuntime::klass_map = NULL;
JCompiler *EnclaveRuntime::compiler = NULL;
std::list<JavaThread*> EnclaveRuntime::threads = std::list<JavaThread*>();

void pre_set_object_alignment() {
    // Object alignment.
    assert(is_power_of_2(ObjectAlignmentInBytes), "ObjectAlignmentInBytes must be power of 2");
    MinObjAlignmentInBytes     = ObjectAlignmentInBytes;
    assert(MinObjAlignmentInBytes >= HeapWordsPerLong * HeapWordSize, "ObjectAlignmentInBytes value is too small");
    MinObjAlignment            = MinObjAlignmentInBytes / HeapWordSize;
    assert(MinObjAlignmentInBytes == MinObjAlignment * HeapWordSize, "ObjectAlignmentInBytes value is incorrect");
    MinObjAlignmentInBytesMask = MinObjAlignmentInBytes - 1;

    LogMinObjAlignmentInBytes  = exact_log2(ObjectAlignmentInBytes);
    LogMinObjAlignment         = LogMinObjAlignmentInBytes - LogHeapWordSize;

    // Oop encoding heap max
    OopEncodingHeapMax = (uint64_t(max_juint) + 1) << LogMinObjAlignmentInBytes;

}

void mutex_init();

extern bool  universe_post_init();

extern void javaClasses_init();

void* EnclaveRuntime::init(void* cpuid, void** heap_top, void** heap_bottom, void** klass_list, int db) {

    debug_bit = db;

    sgx_register_exception_handler(10, signal_handler);

    // set mode
    UseCompiler              = false;
    UseLoopCounter           = false;
    AlwaysCompileLoopMethods = false;
    UseOnStackReplacement    = false;
    UseCompressedClassPointers = false;
    UseCompressedOops = false;
    UseTLAB = false;
    UseSharedSpaces = false;
    pre_set_object_alignment();

    mutex_init();
    chunkpool_init();

    JavaThread javaThread;
    javaThread.initialize_thread_local_storage();
    ThreadLocalStorage::set_thread(&javaThread);

    ClassLoaderData::init_null_class_loader_data();
    SymbolTable::create_table();
    StringTable::create_table();

    char* memory_start = (char*)EnclaveMemory::init();
    EnclaveMemory::heap_top = heap_top;
    EnclaveMemory::heap_bottom = heap_bottom;
    os::set_memory_serialize_page((address)EnclaveMemory::new_page());
    os::set_polling_page((address)EnclaveMemory::new_page());
    // SystemDictionary::_box_klasses = (Klass**)KLASS_get_type_klass();
    // EnclaveMemory::wk_classes = (Klass**)klass_list;

    Abstract_VM_Version::initialize();
    Bytecodes::initialize();
    icache_init();
    VM_Version::set_features(cpuid);
    VM_Version_init();

    JavaClasses::compute_hard_coded_offsets();

    Interpreter::initialize();
    EnclaveABI::init();

    // vmSymbols::initialize(&javaThread);
    Universe::genesis(&javaThread);

    universe_post_init();

    is_init = true;
    EnclaveNative::init();
    EnclaveException::init();

    StubRoutines::initialize1();

    klass_map = new std::map<std::string, Klass*>();

    compiler = JCompiler::create_compiler();

    Runtime0::initialize();

    javaClasses_init();

    // init mpx and set bound
    ENABLE_MPX();

    return memory_start;
}

void* EnclaveRuntime::do_ecall_comp(void *rbx_buf, void *m, int *has_exception) {

    // check if gc is running
    // EnclaveGC::spin_on_gc(false);

    // if (!init)
    // c1_initialize(NULL);
    JavaThread javaThread;
    javaThread.initialize_thread_local_storage();
    ThreadLocalStorage::set_thread(&javaThread);
    javaThread.initialize();
    int functional = ((Method*)m)->access_flags().is_funtional();
    sgx_spin_lock(&count_mutex);
    in_enclave_thread_count += 1;
    if (in_enclave_thread_count < 2) {
//        printf("funtional\n");
        if (functional)
            EnclaveGC::reset();
    }
    // add thread to the thread list
    threads.push_back(&javaThread);
    sgx_spin_unlock(&count_mutex);

    EnclaveGC::spin_on_gc(true);

//    if (clean_pointer != NULL) {
//        void* head;
//        while (clean_pointer->size() > 0) {
//            head = clean_pointer->front();
//            free(head);
//            clean_pointer->pop_front();
//        }
//        delete clean_pointer;
//    }

    const char* classname = ((Method*)m)->klass_name()->as_C_string();
    Symbol *csym = SymbolTable::new_symbol(classname, &javaThread);
    // ClassLoader::load_classfile(csym, &javaThread);
    Klass *c = SystemDictionary::resolve_or_null(csym, Handle(), Handle(), &javaThread);

    CallInfo info;
    KlassHandle kh(c);
    Symbol *signature = SymbolTable::new_symbol(((Method*)m)->signature()->as_C_string(), &javaThread);
    Symbol *name = SymbolTable::new_symbol(((Method*)m)->name()->as_C_string(), &javaThread);
    LinkResolver::resolve_static_call(info, kh, name, signature, KlassHandle(), false, true, &javaThread);
    u_char* r = NULL;
    {
        r = (u_char*)call_compiler(rbx_buf, (Method*)info.resolved_method()());
    }
    if (javaThread.has_unhandle_exception()) {
        *has_exception = 1;
    }
    sgx_spin_lock(&count_mutex);
    in_enclave_thread_count -= 1;
    threads.remove(&javaThread);
    sgx_spin_unlock(&count_mutex);
    return (void*)r;
}

void* EnclaveRuntime::call_compiler(void *rbx_buf, Method *m) {
    compile_method(m);
    return call_interpreter(rbx_buf, m);
}

void* EnclaveRuntime::call_interpreter(void *rbx_buf, Method *m) {
    ecall_count += 1;
    if ((EnclaveRuntime::debug_bit & debug_ecall) == debug_ecall) {
        printf("ecall %d, ocall %d\n", ecall_count, ocall_count);
    }
    JavaThread *javaThread = JavaThread::current();
    char rsp_tmp[8];
    return EnclaveABI::do_ecall(rbx_buf, rsp_tmp, javaThread, (address)m);
}

void* EnclaveRuntime::compile_method(Method *method) {
    compiler->compile_method(method);
}

int EnclaveRuntime::signal_handler(sgx_exception_info_t *info) {
    // switch (info->exception_vector) {
    //     case SGX_EXCEPTION_VECTOR_DE:
    //         info->cpu_context.rip = (u_int64_t)Interpreter::throw_ArithmeticException_entry();
    //         return -1;
    //     case SGX_EXCEPTION_VECTOR_SEV:
    //         if (info->cpu_context.r10 == 0xfe3f912) {
    //             info->cpu_context.rip = (u_int64_t)Interpreter::throw_StackOverflowError_entry();
    //         } else {
    //             info->cpu_context.rip = (u_int64_t) Interpreter::throw_NullPointerException_entry();
    //         }
    //         return -1;
    //     default:
    //         break;
    // }
    return 0;
}
