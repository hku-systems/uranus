/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

/*
 * We used part of Netscape's Java Runtime Interface (JRI) as the starting
 * point of our design and implementation.
 */

/******************************************************************************
 * Java Runtime Interface
 * Copyright (c) 1996 Netscape Communications Corporation. All rights reserved.
 *****************************************************************************/

#ifndef _JAVASOFT_JNI_H_
#define _JAVASOFT_JNI_H_

#include <stdio.h>
#include <stdarg.h>

/* jni_md.h contains the machine-dependent typedefs for jbyte, jint
   and jlong */

#include "jni_md.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * JNI Types
 */

#ifndef JNI_TYPES_ALREADY_DEFINED_IN_JNI_MD_H

typedef unsigned char   jboolean;
typedef unsigned short  jchar;
typedef short           jshort;
typedef float           jfloat;
typedef double          jdouble;

typedef jint            jsize;

#ifdef __cplusplus

class _jobject {};
class _jclass : public _jobject {};
class _jthrowable : public _jobject {};
class _jstring : public _jobject {};
class _jarray : public _jobject {};
class _jbooleanArray : public _jarray {};
class _jbyteArray : public _jarray {};
class _jcharArray : public _jarray {};
class _jshortArray : public _jarray {};
class _jintArray : public _jarray {};
class _jlongArray : public _jarray {};
class _jfloatArray : public _jarray {};
class _jdoubleArray : public _jarray {};
class _jobjectArray : public _jarray {};

typedef _jobject *jobject;
typedef _jclass *jclass;
typedef _jthrowable *jthrowable;
typedef _jstring *jstring;
typedef _jarray *jarray;
typedef _jbooleanArray *jbooleanArray;
typedef _jbyteArray *jbyteArray;
typedef _jcharArray *jcharArray;
typedef _jshortArray *jshortArray;
typedef _jintArray *jintArray;
typedef _jlongArray *jlongArray;
typedef _jfloatArray *jfloatArray;
typedef _jdoubleArray *jdoubleArray;
typedef _jobjectArray *jobjectArray;

#else

struct _jobject;

typedef struct _jobject *jobject;
typedef jobject jclass;
typedef jobject jthrowable;
typedef jobject jstring;
typedef jobject jarray;
typedef jarray jbooleanArray;
typedef jarray jbyteArray;
typedef jarray jcharArray;
typedef jarray jshortArray;
typedef jarray jintArray;
typedef jarray jlongArray;
typedef jarray jfloatArray;
typedef jarray jdoubleArray;
typedef jarray jobjectArray;

#endif

typedef jobject jweak;

typedef union jvalue {
    jboolean z;
    jbyte    b;
    jchar    c;
    jshort   s;
    jint     i;
    jlong    j;
    jfloat   f;
    jdouble  d;
    jobject  l;
} jvalue;

struct _jfieldID;
typedef struct _jfieldID *jfieldID;

struct _jmethodID;
typedef struct _jmethodID *jmethodID;

/* Return values from jobjectRefType */
typedef enum _jobjectType {
     JNIInvalidRefType    = 0,
     JNILocalRefType      = 1,
     JNIGlobalRefType     = 2,
     JNIWeakGlobalRefType = 3
} jobjectRefType;


#endif /* JNI_TYPES_ALREADY_DEFINED_IN_JNI_MD_H */

/*
 * jboolean constants
 */

#define JNI_FALSE 0
#define JNI_TRUE 1

/*
 * possible return values for JNI functions.
 */

#define JNI_OK           0                 /* success */
#define JNI_ERR          (-1)              /* unknown error */
#define JNI_EDETACHED    (-2)              /* thread detached from the VM */
#define JNI_EVERSION     (-3)              /* JNI version error */
#define JNI_ENOMEM       (-4)              /* not enough memory */
#define JNI_EEXIST       (-5)              /* VM already created */
#define JNI_EINVAL       (-6)              /* invalid arguments */

/*
 * used in ReleaseScalarArrayElements
 */

#define JNI_COMMIT 1
#define JNI_ABORT 2

/*
 * used in RegisterNatives to describe native method name, signature,
 * and function pointer.
 */

typedef struct {
    char *name;
    char *signature;
    void *fnPtr;
} JNINativeMethod;

/*
 * JNI Native Method Interface.
 */

struct JNINativeInterface_;

/*
 * JNI Invocation Interface.
 */

struct JNIInvokeInterface_;

struct JavaVM_;

struct JNIEnv;

typedef struct JNIEnv JNIEnv;

struct JNIEnv {
    jclass (JNICALL *GetSuperclass)
            (JNIEnv *env, jclass sub);
    jboolean (JNICALL *IsAssignableFrom)
            (JNIEnv *env, jclass sub, jclass sup);
    jclass (JNICALL *GetObjectClass)
            (JNIEnv *env, jobject obj);
    jboolean(JNICALL *IsInstanceOf)
            (JNIEnv *env, jobject obj, jclass clazz);

    jsize (JNICALL *GetArrayLength)
            (JNIEnv *env, jarray array);

    void (JNICALL *GetBooleanArrayRegion)
            (JNIEnv *env, jbooleanArray array, jsize start, jsize l, jboolean *buf);
    void (JNICALL *GetByteArrayRegion)
            (JNIEnv *env, jbyteArray array, jsize start, jsize len, jbyte *buf);
    void (JNICALL *GetCharArrayRegion)
            (JNIEnv *env, jcharArray array, jsize start, jsize len, jchar *buf);
    void (JNICALL *GetShortArrayRegion)
            (JNIEnv *env, jshortArray array, jsize start, jsize len, jshort *buf);
    void (JNICALL *GetIntArrayRegion)
            (JNIEnv *env, jintArray array, jsize start, jsize len, jint *buf);
    void (JNICALL *GetLongArrayRegion)
            (JNIEnv *env, jlongArray array, jsize start, jsize len, jlong *buf);
    void (JNICALL *GetFloatArrayRegion)
            (JNIEnv *env, jfloatArray array, jsize start, jsize len, jfloat *buf);
    void (JNICALL *GetDoubleArrayRegion)
            (JNIEnv *env, jdoubleArray array, jsize start, jsize len, jdouble *buf);

    void (JNICALL *SetBooleanArrayRegion)
            (JNIEnv *env, jbooleanArray array, jsize start, jsize l, const jboolean *buf);
    void (JNICALL *SetByteArrayRegion)
            (JNIEnv *env, jbyteArray array, jsize start, jsize len, const jbyte *buf);
    void (JNICALL *SetCharArrayRegion)
            (JNIEnv *env, jcharArray array, jsize start, jsize len, const jchar *buf);
    void (JNICALL *SetShortArrayRegion)
            (JNIEnv *env, jshortArray array, jsize start, jsize len, const jshort *buf);
    void (JNICALL *SetIntArrayRegion)
            (JNIEnv *env, jintArray array, jsize start, jsize len, const jint *buf);
    void (JNICALL *SetLongArrayRegion)
            (JNIEnv *env, jlongArray array, jsize start, jsize len, const jlong *buf);
    void (JNICALL *SetFloatArrayRegion)
            (JNIEnv *env, jfloatArray array, jsize start, jsize len, const jfloat *buf);
    void (JNICALL *SetDoubleArrayRegion)
            (JNIEnv *env, jdoubleArray array, jsize start, jsize len, const jdouble *buf);


    jboolean * (JNICALL *GetBooleanArrayElements)
            (JNIEnv *env, jbooleanArray array, jboolean *isCopy);
    jbyte * (JNICALL *GetByteArrayElements)
            (JNIEnv *env, jbyteArray array, jboolean *isCopy);
    jchar * (JNICALL *GetCharArrayElements)
            (JNIEnv *env, jcharArray array, jboolean *isCopy);
    jshort * (JNICALL *GetShortArrayElements)
            (JNIEnv *env, jshortArray array, jboolean *isCopy);
    jint * (JNICALL *GetIntArrayElements)
            (JNIEnv *env, jintArray array, jboolean *isCopy);
    jlong * (JNICALL *GetLongArrayElements)
            (JNIEnv *env, jlongArray array, jboolean *isCopy);
    jfloat * (JNICALL *GetFloatArrayElements)
            (JNIEnv *env, jfloatArray array, jboolean *isCopy);
    jdouble * (JNICALL *GetDoubleArrayElements)
            (JNIEnv *env, jdoubleArray array, jboolean *isCopy);

    void (JNICALL *ReleaseBooleanArrayElements)
            (JNIEnv *env, jbooleanArray array, jboolean *elems, jint mode);
    void (JNICALL *ReleaseByteArrayElements)
            (JNIEnv *env, jbyteArray array, jbyte *elems, jint mode);
    void (JNICALL *ReleaseCharArrayElements)
            (JNIEnv *env, jcharArray array, jchar *elems, jint mode);
    void (JNICALL *ReleaseShortArrayElements)
            (JNIEnv *env, jshortArray array, jshort *elems, jint mode);
    void (JNICALL *ReleaseIntArrayElements)
            (JNIEnv *env, jintArray array, jint *elems, jint mode);
    void (JNICALL *ReleaseLongArrayElements)
            (JNIEnv *env, jlongArray array, jlong *elems, jint mode);
    void (JNICALL *ReleaseFloatArrayElements)
            (JNIEnv *env, jfloatArray array, jfloat *elems, jint mode);
    void (JNICALL *ReleaseDoubleArrayElements)
            (JNIEnv *env, jdoubleArray array, jdouble *elems, jint mode);


};

JNIEnv* jni_functions();

typedef struct JavaVMOption {
    char *optionString;
    void *extraInfo;
} JavaVMOption;

typedef struct JavaVMInitArgs {
    jint version;

    jint nOptions;
    JavaVMOption *options;
    jboolean ignoreUnrecognized;
} JavaVMInitArgs;

typedef struct JavaVMAttachArgs {
    jint version;

    char *name;
    jobject group;
} JavaVMAttachArgs;

/* These will be VM-specific. */

#define JDK1_2
#define JDK1_4

/* End VM-specific. */

#define JNI_VERSION_1_1 0x00010001
#define JNI_VERSION_1_2 0x00010002
#define JNI_VERSION_1_4 0x00010004
#define JNI_VERSION_1_6 0x00010006
#define JNI_VERSION_1_8 0x00010008

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* !_JAVASOFT_JNI_H_ */
