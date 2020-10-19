
#include "precompiled.hpp"
#include "EnclaveDebug.h"
#include "interpreter/interpreterRuntime.hpp"
#include "EnclaveOcall.h"
#include "EnclaveException.h"
#include "EnclaveNative.h"
#include "libmpx/mpxrt.h"


#define DO_OCALL_CHECK(func) \
    sgx_status_t call_status = func; \
    if (call_status != SGX_SUCCESS) { \
        printf(D_ERROR("DO_OCALL")" ocall %s, error code %d\n", __FUNCTION__, call_status); \
    } \
    ENABLE_MPX(); \
    OCallTrace

void* JVM_ENTRY_omalloc(int size OCallCallerArgs) {
    char* ret;
    DO_OCALL_CHECK(ocall_jvm_malloc(&ret, size));
    return ret;
}

void KLASS_compute_oopmap(void* klass, void* method, int bci OCallCallerArgs) {
    // TODO: do computation at need / in encalve?
    DO_OCALL_CHECK(ocall_klass_compute_oopmap(klass, method, bci));
}

void* JVM_CLASSFILE_BUFFER(const char* name, int *size) {
    char* ret = NULL;
    DO_OCALL_CHECK(ocall_classfile_buffer(&ret, name, size));
    return ret;
}