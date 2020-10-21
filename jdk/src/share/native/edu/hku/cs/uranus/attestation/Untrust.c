#include "jni.h"
#include "jvm.h"
#include "edu_hku_cs_uranus_attestation_Untrust.h"

JNIEXPORT jbyteArray JNICALL
Java_edu_hku_cs_uranus_attestation_Untrust_process_1msg01(JNIEnv *env, jobject obj, jbyteArray msg0, jbyteArray msg1)
{
    return NULL;
}

JNIEXPORT jbyteArray JNICALL
Java_edu_hku_cs_uranus_attestation_Untrust_process_1msg3(JNIEnv *env, jobject obj, jbyteArray msg3)
{
    // get the jar hash
    jfieldID fid;
    jbyteArray jar_hash;

    jclass cls = (*env)->GetObjectClass(env, obj);
    fid = (*env)->GetFieldID(env, cls, "jar_hash", "[C");
    if (fid == NULL)
    {
        return 0; /* failed to find the field */
    }

    /* Read the instance field s */
    jar_hash = (jbyteArray)(*env)->GetObjectField(env, obj, fid);

    jchar *jar_hash_c = (*env)->GetCharArrayElements(env, jar_hash, 0);
    jchar *jar_hash_remote_c = (*env)->GetCharArrayElements(env, msg3, 0);

    jboolean status = JNI_TRUE;

    for (int i = 0;i < 32;i++) {
        if (jar_hash_c[i] != jar_hash_remote_c[i]) {
            status = JNI_FALSE;
            break;
        }
    }

    (*env)->ReleaseCharArrayElements(env, jar_hash, jar_hash_c, 0);
    (*env)->ReleaseCharArrayElements(env, msg3, jar_hash_remote_c, 0);

    if (status == JNI_TRUE) return msg3;
    return NULL;
}

JNIEXPORT jboolean JNICALL
Java_edu_hku_cs_uranus_attestation_Untrust_process_1msg4(JNIEnv *env, jobject obj, jbyteArray msg4)
{
    if (msg4 == NULL) return JNI_FALSE;
    return JNI_TRUE;
}