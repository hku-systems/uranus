
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
    DO_OCALL_CHECK(ocall_jvm_malloc((void**)&ret, size));
    return ret;
}

void JVM_ENTRY_resolve_invoke(void* th, int bytecode OCallCallerArgs) {
    JavaThread* thread = (JavaThread*)th;
    JavaThread* __the_thread__ = thread;
    void *method = InterpreterRuntime::method(thread);
    int bci = InterpreterRuntime::bci(thread);
    void* bcp = InterpreterRuntime::bcp(thread);
    int idx = InterpreterRuntime::get_index_u2_cpcache(thread, (Bytecodes::Code)bytecode);
    Bytecode_invoke call((Method*)method, bci);
    char* recv = NULL;
    Klass* recv_klass = NULL;
    if (bytecode == Bytecodes::_invokevirtual || bytecode == Bytecodes::_invokeinterface ||
        bytecode == Bytecodes::_invokespecial) {
        recv = (char*)thread->last_frame().interpreter_callee_receiver(call.signature());
        if (recv == NULL) {
          ENCLAVE_THROW(EnclaveException::java_lang_NullPointerException);
        }
        recv_klass = ((oop)recv)->klass();
    }
    DO_OCALL_CHECK(ocall_jvm_resolve_invoke(bytecode, method, bci, recv, idx, bcp, (void*)recv_klass));
}

void* JVM_ENTRY_resolve_invoke_C(void* recv, void* constant, int bidx, int bc OCallCallerArgs) {
    char* ret;
    oop receiver = (oop)recv;
    Klass* recv_klass = NULL;
    JavaThread* THREAD = JavaThread::current();
    Bytecodes::Code bytecode = (Bytecodes::Code)bc;
    if (bytecode == Bytecodes::_invokevirtual || bytecode == Bytecodes::_invokeinterface ||
        bytecode == Bytecodes::_invokespecial) {
        if (recv == NULL) {
            ENCLAVE_THROW_0(EnclaveException::java_lang_NullPointerException);
        }
        recv_klass = receiver->klass();
    }
    DO_OCALL_CHECK(ocall_jvm_resolve_invoke_C((void**)&ret, recv, constant, bidx, bc, recv_klass));
    return ret;
}

void* JVM_ENTRY_resolve_method_C(int bc, void* constant, int idx OCallCallerArgs) {
    char* ret;
    DO_OCALL_CHECK(ocall_jvm_resolve_method_C((void**)&ret, bc, constant, idx));
    return ret;
}

void JVM_ENTRY_resolve_invoke_handle(void* thread OCallCallerArgs) {
    DO_OCALL_CHECK(ocall_jvm_resolve_invoke_handle());
}

void JVM_ENTRY_resolve_invoke_dynamic(void* thread OCallCallerArgs) {
    DO_OCALL_CHECK(ocall_jvm_resolve_invoke_dynamic());
}

void* JVM_ENTRY_ldc(void* thread, bool wide, void *p, int index OCallCallerArgs) {
    char* ret = NULL;
    ConstantPool *pool = (ConstantPool*)p;
    intptr_t entry = (intptr_t)OrderAccess::load_ptr_acquire(pool->obj_at_addr_raw(index));
    if ((entry & 1) == 0) {
        return ((Klass*)entry)->java_mirror();
    }
    DO_OCALL_CHECK(ocall_jvm_ldc((void**)&ret, wide, p, index));
    return ret;
}

void* JVM_ENTRY_resolve_ldc(void* thread,void* p, int index, int bytecode OCallCallerArgs) {
    char* ret = NULL;
    ConstantPool *pool = (ConstantPool*)p;
//    if (index >= 0) {
//        oop result = pool->resolved_references()->obj_at(index);
//        if (result != NULL)
//            return result;
//    }
    DO_OCALL_CHECK(ocall_jvm_resolve_ldc((void**)&ret, p, index, bytecode));
    return ret;
}

void* KLASS_get_type_klass(void* dummy OCallCallerArgs) {
    char *ret = NULL;
    DO_OCALL_CHECK(ocall_klass_type((void**)&ret));
    return ret;
}

void* KLASS_get_type_array_klass(void* dummy OCallCallerArgs) {
    char *ret = NULL;
    DO_OCALL_CHECK(ocall_klass_type_array((void**)&ret));
    return ret;
}

Klass* get_array_klass(Klass* klass, int dim) {
    if (klass == NULL) {
        return NULL;
    } else if (klass->oop_is_instance()) {
       Klass* ak = ((InstanceKlass*)klass)->array_klasses();
       if (ak != NULL) {
           if (dim == ((ArrayKlass*)ak)->_dimension)
            return ak;
           else
            return get_array_klass(ak, dim);
       } else {
           return NULL;
       }
    } else if (klass->oop_is_array()) {
        if (dim == ((ArrayKlass*)klass)->_dimension) {
            return klass;
        } else {
            return get_array_klass(((ObjArrayKlass*)klass)->_higher_dimension, dim);
        }
    } else {
        ShouldNotReachHere();
    }
    return NULL;
}

void* KLASS_get_obj_array_klass(void* pool, int index OCallCallerArgs) {
    char *ret = NULL;
    ConstantPool *p = (ConstantPool*)pool;
    intptr_t entry = (intptr_t)OrderAccess::load_ptr_acquire(p->obj_at_addr_raw(index));
    Klass *super = NULL;
    if ((entry & 1) == 0) {
        Klass* base_klass = (Klass*)entry;
        int dim = 0;
        if (base_klass->oop_is_array()) {
            dim = ((ArrayKlass*)base_klass)->dimension() + 1;
        } else {
            dim = 1;
        }
        super = get_array_klass(base_klass, dim);
    }
    if (super == NULL) {
        DO_OCALL_CHECK(ocall_obj_array_klass_get((void**)&ret, pool, index));
        return ret;
    }
    return super;
}

void* KLASS_get_multi_array_klass(void* pool, int index OCallCallerArgs) {
    char *ret = NULL;
    ConstantPool *p = (ConstantPool*)pool;
    intptr_t entry = (intptr_t)OrderAccess::load_ptr_acquire(p->obj_at_addr_raw(index));
    if ((entry & 1) == 0) {
        return (Klass*)entry;
    }
    DO_OCALL_CHECK(ocall_multi_array_klass_get((void**)&ret, pool, index));
    return ret;
}

void* KLASS_resolve_or_fail(const char* name OCallCallerArgs) {
  char* ret = NULL;
  DO_OCALL_CHECK(ocall_klass_resolve_or_fail((void**)&ret, (char*)name));
  Klass* k = (Klass*)ret;
  if (ret != NULL) {
    EnclaveRuntime::insert_klass_map(k->name(), k);
  }
  return ret;
}

void* KLASS_find_or_null(const char* name OCallCallerArgs) {
    char* ret = NULL;
    // TODO
    return NULL;
    DO_OCALL_CHECK(ocall_klass_find_or_null((void**)&ret, (char*)name));
    Klass* k = (Klass*)ret;
    return ret;
}

// TODO pass thread into this func
void* JVM_ENTRY_resolve_klass(void* thread, void* pool, int index OCallCallerArgs) {
    char* ret = NULL;
    DO_OCALL_CHECK(ocall_klass_get((void**)&ret, pool, index));
    return ret;
}

void* JVM_ENTRY_quick_cc(void* thread, void* pool, int index OCallCallerArgs) {
    char* ret = NULL;
    DO_OCALL_CHECK(ocall_klass_get((void**)&ret, pool, -index));
    return ret;
}

void JVM_ENTRY_resolve_get_put(void* th, int bytecode OCallCallerArgs) {
    JavaThread* thread = (JavaThread*)th;
    Method* method = InterpreterRuntime::method(thread);
    int idx = InterpreterRuntime::get_index_u2_cpcache(thread, (Bytecodes::Code)bytecode);
    void* bcp = InterpreterRuntime::bcp(thread);
    int ret;
    DO_OCALL_CHECK(ocall_jvm_resolve_get_put(&ret, bytecode, method, idx, bcp));
}

int JVM_ENTRY_resolve_get_put_C(void* thread, void* method, int index, int bytecode OCallCallerArgs) {
    int ret;
    DO_OCALL_CHECK(ocall_jvm_resolve_get_put(&ret, -bytecode, method, index, 0));
    return ret;
}

void* Klass_resolve_get_put(void* thread, void* method, int index, int bytecode OCallCallerArgs) {
    char* ret;
    DO_OCALL_CHECK(ocall_jvm_resolve_get_put_klass((void**)&ret, bytecode, method, index, NULL));
    return ret;
}

void JVM_ENTRY_pre_native(void* thread, void* method, bool resolve OCallCallerArgs) {
//    if (resolve)
//        ocall_jvm_pre_native(method, (int)resolve);
    if (!resolve)
        EnclaveNative::resolve_function((Method*)method);
    SignatureHandlerLibrary::add(methodHandle((Method*)method));
}

void* KLASS_array_klass(void* klass, int rank, int get_all OCallCallerArgs) {
    // TODO: do resolve at need
    // handle by jianyu
    // need to check if this is the case
    char* ret = NULL;
    Klass* super = get_array_klass((Klass*)klass, rank);
    if (super != NULL)
        return super;
    DO_OCALL_CHECK(ocall_array_klass((void**)&ret, klass, rank, get_all));
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