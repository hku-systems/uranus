#include "jni.h"
#include "jvm.h"
#include "edu_hku_cs_uranus_attestation_Enclave.h"

// static JNINativeMethod methods[] = {
//     {"sgx_ra_init",         "()V",          (void *)&Java_edu_hku_cs_uranus_attestation_Enclave_sgx_ra_init},
//     {"sgx_ra_get_msg1",     "([C;)B",       (void *)&Java_edu_hku_cs_uranus_attestation_Enclave_sgx_ra_get_msg1},
//     {"sgx_ra_proc_msg2",    "([C;[C;)B",    (void *)&Java_edu_hku_cs_uranus_attestation_Enclave_sgx_ra_proc_msg2},
//     {"sgx_ra_proc_msg4",    "([C;[C;)B",    (void *)&Java_edu_hku_cs_attestation_uranus_Enclave_sgx_ra_proc_msg4},
// };

JNIEXPORT jboolean JNICALL
Java_edu_hku_cs_uranus_attestation_Enclave_sgx_1ra_1init(JNIEnv *env, jobject obj)
{
    return JNI_TRUE;
}

JNIEXPORT jcharArray JNICALL
Java_edu_hku_cs_uranus_attestation_Enclave_sgx_1ra_1get_1msg0(JNIEnv *env, jobject obj)
{
    return NULL;
}

JNIEXPORT jcharArray JNICALL
Java_edu_hku_cs_uranus_attestation_Enclave_sgx_1ra_1get_1msg1(JNIEnv *env, jobject obj)
{
    return NULL;
}

JNIEXPORT jcharArray JNICALL
Java_edu_hku_cs_uranus_attestation_Enclave_sgx_1ra_1proc_1msg2(JNIEnv *env, jobject obj, jbyteArray msg2)
{
    return JVM_GetEnclaveJarHash(env, obj);
}

JNIEXPORT jboolean JNICALL
Java_edu_hku_cs_uranus_attestation_Enclave_sgx_1ra_1proc_1msg4(JNIEnv *env, jobject obj, jbyteArray msg4)
{
    return JNI_TRUE;
}