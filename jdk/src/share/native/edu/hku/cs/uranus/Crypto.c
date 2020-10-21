
#include "jni.h"
#include "jvm.h"
#include "edu_hku_cs_uranus_Crypto.h"

/*
 * Cryptography Api for SGX
 */
JNIEXPORT jbyteArray JNICALL
Java_edu_anonymity_sgx_Crypto_sgx_encrypt(JNIEnv *env, jclass ignore, jbyteArray array, jboolean en)
{
//    return JVM_BytesEncrypt(env, ignore, array);
}

JNIEXPORT jbyteArray JNICALL
Java_edu_anonymity_sgx_Crypto_sgx_decrypt(JNIEnv *env, jclass ignore, jbyteArray array, jboolean en)
{
//    return JVM_BytesDecrypt(env, ignore, array);
}

JNIEXPORT jbyteArray JNICALL
Java_edu_anonymity_sgx_Crypto_sgx_encrypt_int(JNIEnv *env, jclass ignore, jintArray array, jboolean en)
{
//    return JVM_BytesDecrypt(env, ignore, array);
}

JNIEXPORT jintArray JNICALL
Java_edu_anonymity_sgx_Crypto_sgx_decrypt_int(JNIEnv *env, jclass ignore, jbyteArray array, jboolean en)
{
//    return JVM_BytesDecrypt(env, ignore, array);
}

JNIEXPORT jbyteArray JNICALL
Java_edu_anonymity_sgx_Crypto_sgx_encrypt_double(JNIEnv *env, jclass ignore, jdoubleArray array, jboolean en)
{
//    return JVM_BytesDecrypt(env, ignore, array);
}

JNIEXPORT jdoubleArray JNICALL
Java_edu_anonymity_sgx_Crypto_sgx_decrypt_double(JNIEnv *env, jclass ignore, jbyteArray array, jboolean en)
{
//    return JVM_BytesDecrypt(env, ignore, array);
}

JNIEXPORT jbyteArray JNICALL
Java_edu_anonymity_sgx_Crypto_sgx_hash(JNIEnv *env, jclass ignore, jbyteArray array, jboolean en)
{
//    return JVM_BytesDecrypt(env, ignore, array);
}

JNIEXPORT jboolean JNICALL
Java_edu_anonymity_sgx_Crypto_sgx_verify(JNIEnv *env, jclass ignore, jbyteArray array, jbyteArray hash)
{
//    return JVM_BytesDecrypt(env, ignore, array);
}