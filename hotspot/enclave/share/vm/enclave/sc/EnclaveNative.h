//
// Created by Maxxie Jiang on 31/1/2018.
//

#ifndef HOTSPOT_SHAREDHEADER_H
#define HOTSPOT_SHAREDHEADER_H

#include <precompiled.hpp>
#include "native_libs/java_lang_Thread.h"
#include "native_libs/sun_misc_Unsafe.h"

#define ENCLAVE_NATIVE_EUNM(name)   name
#define VM_ENCLAVE_ENUM(name, string, func) ENCLAVE_NATIVE_EUNM(name),
#define VM_ENCLAVE_ENTRY(name, string, func)    native_name[name] = (char*)(string);        \
                                                native_entry[name] = CAST_FROM_FN_PTR(void*, func);
#define RE_TEMPLATE(name, template) template(enum_##name, #name, &name)
#define StrictMathTemplate(name, template) RE_TEMPLATE(Java_java_lang_StrictMath_##name, template)
#define DO_STRICTMATH(template) \
    StrictMathTemplate(cos, template) StrictMathTemplate(sin, template) StrictMathTemplate(tan, template)     \
    StrictMathTemplate(asin, template) StrictMathTemplate(acos, template) StrictMathTemplate(atan, template)  \
    StrictMathTemplate(exp, template) StrictMathTemplate(log, template) StrictMathTemplate(log10, template)   \
    StrictMathTemplate(sqrt, template) StrictMathTemplate(cbrt, template) StrictMathTemplate(atan2, template) \
    StrictMathTemplate(pow, template) StrictMathTemplate(cosh, template) StrictMathTemplate(sinh, template) \
    StrictMathTemplate(tanh, template)

#define DO_ENCLAVE_NATIVE(template) \
    template(system_arraycopy,                  "Java_java_lang_System_arraycopy",                      &JVM_ArrayCopy)                     \
    template(system_identityHash,               "Java_java_lang_System_identityHashCode",               &System_identityHashCode)           \
    template(object_clone,                      "Java_java_lang_Object_clone",                          &JVM_Clone)                         \
    template(object_hashcode,                   "Java_java_lang_Object_hashCode",                       &JVM_IHashCode)                     \
    template(object_notifyAll,                  "Java_java_lang_Object_notifyAll",                      &JVM_MonitorNotifyAll)              \
    template(object_notify,                     "Java_java_lang_Object_notify",                         &JVM_MonitorNotify)                 \
    template(array_new,                         "Java_java_lang_reflect_Array_newArray",                &JVM_EnclaveNewArray)               \
    template(array_multi_new,                   "Java_java_lang_reflect_Array_multiNewArray",           &JVM_EnclaveNewMultiArray)          \
    template(array_get,                         "NULL",                                                 NULL)                               \
    template(array_set,                         "Java_java_lang_reflect_Array_set",                     &JVM_EnclaveSetArrayElement)        \
    template(class_get,                         "Java_java_lang_Object_getClass",                       jni_functions()->GetObjectClass)    \
    template(class_getName,                     "Java_java_lang_Class_getName0",                        &JVM_GetClassName)                  \
    template(class_getSuper,                    "Java_java_lang_Class_getSuperclass",                   jni_functions()->GetSuperclass)     \
    template(class_component,                   "Java_java_lang_Class_getComponentType",                &JVM_GetComponentType)              \
    template(class_isInstance,                  "Java_java_lang_Class_isInstance",                      &JVM_IsInstanceOf)                  \
    template(class_isArray,                     "Java_java_lang_Class_isArray",                         &JVM_IsArrayClass)                  \
    template(class_isInterface,                 "Java_java_lang_Class_isInterface",                     &JVM_IsInterface)                   \
    template(class_isPrimitive,                 "Java_java_lang_Class_isPrimitive",                     &JVM_IsPrimitiveClass)              \
    template(class_isAssignableFrom,            "Java_java_lang_Class_isAssignableFrom",                jni_functions()->IsAssignableFrom)  \
    template(class_getInterfaces,               "Java_java_lang_Class_getInterfaces0",                  &JVM_GetClassInterfaces)            \
    template(throwable_FillInStackTrace,        "Java_java_lang_Throwable_fillInStackTrace",            &JVM_FillInStackTrace)              \
    template(throwable_GetStackTraceDepth,      "Java_java_lang_Throwable_getStackTraceDepth",          &JVM_GetStackTraceDepth)            \
    template(throwable_GetStackTraceElement,    "Java_java_lang_Throwable_getStackTraceElement",        &JVM_GetStackTraceElement)          \
    template(tools_print,                       "Java_edu_anonymity_sgx_Tools_print",                   &JVM_EnclaveDebug)                  \
    template(tools_clone,                       "Java_edu_anonymity_sgx_Tools_copy_1out",               &JVM_EnclaveCopy)                   \
    template(tools_deep_copy,                   "Java_edu_anonymity_sgx_Tools_deep_1copy",              &JVM_EnclaveDeepCopy)               \
    template(tools_clean,                       "Java_edu_anonymity_sgx_Tools_clean",                   &JVM_EnclaveClean)                  \
    template(thread_currentThread,              "Java_java_lang_Thread_currentThread",                  &JVM_CurrentThread)                 \
    template(thread_yield,                      "Java_java_lang_Thread_yield",                          &Java_java_lang_Thread_yield)       \
    template(sgx_encrypt,                       "Java_edu_anonymity_sgx_Crypto_sgx_1encrypt",           &JVM_BytesEncrypt)                  \
    template(sgx_decrypt,                       "Java_edu_anonymity_sgx_Crypto_sgx_1decrypt",           &JVM_BytesDecrypt)                  \
    template(sgx_encrypt_int,                   "Java_edu_anonymity_sgx_Crypto_sgx_1encrypt_1int",      &JVM_BytesEncryptInt)               \
    template(sgx_decrypt_int,                   "Java_edu_anonymity_sgx_Crypto_sgx_1decrypt_1int",      &JVM_BytesDecryptInt)               \
    template(sgx_encrypt_double,                "Java_edu_anonymity_sgx_Crypto_sgx_1encrypt_1double",   &JVM_BytesEncryptDouble)            \
    template(sgx_decrypt_double,                "Java_edu_anonymity_sgx_Crypto_sgx_1decrypt_1double",   &JVM_BytesDecryptDouble)            \
    template(sgx_hash,                          "Java_edu_anonymity_sgx_Crypto_sgx_1hash",              &JVM_BytesHash)                     \
    template(sgx_verify,                        "Java_edu_anonymity_sgx_Crypto_sgx_1verify",            &JVM_BytesHashVerify)               \
    template(sun_unsafe_compareSwapObject,      "Java_sun_misc_Unsafe_compareAndSwapObject",            &Unsafe_CompareAndSwapObject)       \
    template(sun_unsafe_compareSwapLong,        "Java_sun_misc_Unsafe_compareAndSwapLong",              &Unsafe_CompareAndSwapLong)         \
    template(sun_unsafe_compareSwapInt,         "Java_sun_misc_Unsafe_compareAndSwapInt",               &Unsafe_CompareAndSwapInt)          \
    template(sun_unsafe_getObject,              "Java_sun_misc_Unsafe_getObject",                       &Unsafe_GetObject)                  \
    template(sun_unsafe_getLong,                "Java_sun_misc_Unsafe_getLong",                         &Unsafe_GetLong)                    \
    template(sun_unsafe_getInt,                 "Java_sun_misc_Unsafe_getInt",                          &Unsafe_GetInt)                     \
    template(sun_unsafe_getShort,               "Java_sun_misc_Unsafe_getShort",                        &Unsafe_GetShort)                   \
    template(sun_unsafe_getFloat,               "Java_sun_misc_Unsafe_getFloat",                        &Unsafe_GetFloat)                   \
    template(sun_unsafe_getDouble,              "Java_sun_misc_Unsafe_getDouble",                       &Unsafe_GetDouble)                  \
    template(sun_unsafe_getChar,                "Java_sun_misc_Unsafe_getChar",                         &Unsafe_GetChar)                    \
    template(sun_unsafe_getByte,                "Java_sun_misc_Unsafe_getByte",                         &Unsafe_GetByte)                    \
    template(sun_unsafe_getObjectVolatile,      "Java_sun_misc_Unsafe_getObjectVolatile",               &Unsafe_GetObjectVolatile)          \
    template(sun_unsafe_getIntVolatile,         "Java_sun_misc_Unsafe_getIntVolatile",                  &Unsafe_GetIntVolatile)             \
    template(sun_unsafe_getLongVolatile,        "Java_sun_misc_Unsafe_getLongVolatile",                 &Unsafe_GetLongVolatile)            \
    template(sun_unsafe_getShortVolatile,       "Java_sun_misc_Unsafe_getShortVolatile",                &Unsafe_GetShortVolatile)           \
    template(sun_unsafe_getFloatVolatile,       "Java_sun_misc_Unsafe_getFloatVolatile",                &Unsafe_GetFloatVolatile)           \
    template(sun_unsafe_getByteVolatile,        "Java_sun_misc_Unsafe_getByteVolatile",                 &Unsafe_GetByteVolatile)            \
    template(sun_unsafe_getCharVolatile,        "Java_sun_misc_Unsafe_getCharVolatile",                 &Unsafe_GetCharVolatile)            \
    template(sun_unsafe_getDoubleVolatile,      "Java_sun_misc_Unsafe_getDoubleVolatile",               &Unsafe_GetDoubleVolatile)          \
    template(sun_unsafe_putObject,              "Java_sun_misc_Unsafe_putObject",                       &Unsafe_SetObject)                  \
    template(sun_unsafe_putLong,                "Java_sun_misc_Unsafe_putLong",                         &Unsafe_SetLong)                    \
    template(sun_unsafe_putInt,                 "Java_sun_misc_Unsafe_putInt",                          &Unsafe_SetInt)                     \
    template(sun_unsafe_putShort,               "Java_sun_misc_Unsafe_putShort",                        &Unsafe_SetShort)                   \
    template(sun_unsafe_putFloat,               "Java_sun_misc_Unsafe_putFloat",                        &Unsafe_SetFloat)                   \
    template(sun_unsafe_putDouble,              "Java_sun_misc_Unsafe_putDouble",                       &Unsafe_SetDouble)                  \
    template(sun_unsafe_putChar,                "Java_sun_misc_Unsafe_putChar",                         &Unsafe_SetChar)                    \
    template(sun_unsafe_putByte,                "Java_sun_misc_Unsafe_putByte",                         &Unsafe_SetByte)                    \
    template(sun_unsafe_putObjectVolatile,      "Java_sun_misc_Unsafe_putObjectVolatile",               &Unsafe_SetObjectVolatile)          \
    template(sun_unsafe_putIntVolatile,         "Java_sun_misc_Unsafe_putIntVolatile",                  &Unsafe_SetIntVolatile)             \
    template(sun_unsafe_putLongVolatile,        "Java_sun_misc_Unsafe_putLongVolatile",                 &Unsafe_SetLongVolatile)            \
    template(sun_unsafe_putShortVolatile,       "Java_sun_misc_Unsafe_putShortVolatile",                &Unsafe_SetShortVolatile)           \
    template(sun_unsafe_putFloatVolatile,       "Java_sun_misc_Unsafe_putFloatVolatile",                &Unsafe_SetFloatVolatile)           \
    template(sun_unsafe_putByteVolatile,        "Java_sun_misc_Unsafe_putByteVolatile",                 &Unsafe_SetByteVolatile)            \
    template(sun_unsafe_putCharVolatile,        "Java_sun_misc_Unsafe_putCharVolatile",                 &Unsafe_SetCharVolatile)            \
    template(sun_unsafe_putDoubleVolatile,      "Java_sun_misc_Unsafe_putDoubleVolatile",               &Unsafe_SetDoubleVolatile)          \
    template(sun_unsafe_copyMemory,             "Java_sun_misc_Unsafe_copyMemory",                      &Unsafe_CopyMemory)                 \
    template(double_long_bit_to_double,         "Java_java_lang_Double_longBitsToDouble",               &Double_longBitsToDouble)           \
    template(double_double_to_long_bit,         "Java_java_lang_Double_doubleToRawLongBits",            &Double_doubleToRawLongBits)        \
    DO_STRICTMATH(template)

extern "C" {

    JNIEXPORT jdouble JNICALL
    Double_longBitsToDouble(JNIEnv *env, jclass unused, jlong v);

    /*
     * Find the bit pattern corresponding to a given double float, NOT collapsing NaNs
     */
    JNIEXPORT jlong JNICALL
    Double_doubleToRawLongBits(JNIEnv *env, jclass unused, jdouble v);

    JNIEXPORT jint JNICALL
    System_identityHashCode(JNIEnv *env, jobject ignore, jobject x);

    jboolean
    JVM_IsInstanceOf(JNIEnv *env, jclass c, jobject o);

}

class EnclaveNative {
public:
    enum Natives {
        DO_ENCLAVE_NATIVE(VM_ENCLAVE_ENUM)
        NATIVE_COUNT
    };

    static char* native_name[NATIVE_COUNT];

    static void* native_entry[NATIVE_COUNT];

    static void init();

    static void* resolve_function(Method*);
};

#endif //HOTSPOT_SHAREDHEADER_H
