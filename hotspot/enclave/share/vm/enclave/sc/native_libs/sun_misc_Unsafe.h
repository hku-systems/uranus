#include "prims/jni.h"
/* Header for class java_lang_StrictMath */

#ifndef _Included_sun_misc_Unsafe
#define _Included_sun_misc_Unsafe
#ifdef __cplusplus
extern "C" {
#endif
  jboolean Unsafe_CompareAndSwapObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject e, jobject x);
  jboolean Unsafe_CompareAndSwapLong(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jlong e, jlong x);
  jboolean Unsafe_CompareAndSwapInt(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jint e, jint x);
  jobject Unsafe_GetObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jlong Unsafe_GetLong(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jint Unsafe_GetInt(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jshort Unsafe_GetShort(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jdouble Unsafe_GetDouble(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jfloat Unsafe_GetFloat(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jchar Unsafe_GetChar(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jbyte Unsafe_GetByte(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jobject Unsafe_GetObjectVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jlong Unsafe_GetLongVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jint Unsafe_GetIntVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jshort Unsafe_GetShortVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jfloat Unsafe_GetFloatVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jbyte Unsafe_GetByteVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jchar Unsafe_GetCharVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  jdouble Unsafe_GetDoubleVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset);
  void Unsafe_SetObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject x_h);
  void Unsafe_SetLong(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jlong x);
  void Unsafe_SetInt(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jint x);
  void Unsafe_SetShort(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jshort x);
  void Unsafe_SetFloat(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jfloat x);
  void Unsafe_SetByte(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jbyte x);
  void Unsafe_SetChar(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jchar x);
  void Unsafe_SetDouble(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jdouble x);
  void Unsafe_SetObjectVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject x_h);
  void Unsafe_SetLongVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jlong x);
  void Unsafe_SetIntVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jint x);
  void Unsafe_SetShortVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jshort x);
  void Unsafe_SetFloatVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jfloat x);
  void Unsafe_SetByteVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jbyte x);
  void Unsafe_SetCharVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jchar x);
  void Unsafe_SetDoubleVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jdouble x);
  void Unsafe_CopyMemory(JNIEnv *env, jobject unsafe, jlong srcAddr, jlong dstAddr, jlong size);
#ifdef __cplusplus
}
#endif

#endif