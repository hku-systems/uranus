/*
 * Copyright (c) 1998, 2013, Oracle and/or its affiliates. All rights reserved.
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

#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <limits.h>

#include "jni.h"
#include "jni_util.h"
#include "jlong.h"
#include "jvm.h"
#include "io_util.h"
#include "io_util_md.h"
#include "java_io_FileSystem.h"
#include "java_io_UnixFileSystem.h"

#if defined(_ALLBSD_SOURCE)
#define dirent64 dirent
#define readdir64_r readdir_r
#define stat64 stat
#define statvfs64 statvfs
#endif

/* -- Field IDs -- */

static struct {
    jfieldID path;
} ids;


JNIEXPORT void JNICALL
Java_java_io_UnixFileSystem_initIDs(JNIEnv *env, jclass cls)
{
    jclass fileClass = (*env)->FindClass(env, "java/io/File");
    if (!fileClass) return;
    ids.path = (*env)->GetFieldID(env, fileClass,
                                  "path", "Ljava/lang/String;");
}

/* -- Path operations -- */

extern int canonicalize(char *path, const char *out, int len);

JNIEXPORT jstring JNICALL
Java_java_io_UnixFileSystem_canonicalize0(JNIEnv *env, jobject this,
                                          jstring pathname)
{
    jstring rv = NULL;

    // TODO
    return rv;
}


/* -- Attribute accessors -- */


static jboolean
statMode(const char *path, int *mode)
{
    struct stat64 sb;
    // TODO
    return JNI_FALSE;
}


JNIEXPORT jint JNICALL
Java_java_io_UnixFileSystem_getBooleanAttributes0(JNIEnv *env, jobject this,
                                                  jobject file)
{
    jint rv = 0;

    // TODO
    return rv;
}

JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_checkAccess(JNIEnv *env, jobject this,
                                        jobject file, jint a)
{
    jboolean rv = JNI_FALSE;
    // TODO
    return rv;
}


JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_setPermission(JNIEnv *env, jobject this,
                                          jobject file,
                                          jint access,
                                          jboolean enable,
                                          jboolean owneronly)
{
    jboolean rv = JNI_FALSE;

    // TODO
    return rv;
}

JNIEXPORT jlong JNICALL
Java_java_io_UnixFileSystem_getLastModifiedTime(JNIEnv *env, jobject this,
                                                jobject file)
{
    jlong rv = 0;

    // TODO
    return rv;
}


JNIEXPORT jlong JNICALL
Java_java_io_UnixFileSystem_getLength(JNIEnv *env, jobject this,
                                      jobject file)
{
    jlong rv = 0;

    // TODO
    return rv;
}


/* -- File operations -- */


JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_createFileExclusively(JNIEnv *env, jclass cls,
                                                  jstring pathname)
{
    jboolean rv = JNI_FALSE;

    // TODO
    return rv;
}


JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_delete0(JNIEnv *env, jobject this,
                                    jobject file)
{
    jboolean rv = JNI_FALSE;

    // TODO
    return rv;
}


JNIEXPORT jobjectArray JNICALL
Java_java_io_UnixFileSystem_list(JNIEnv *env, jobject this,
                                 jobject file)
{
    // TODO
    return NULL;
}


JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_createDirectory(JNIEnv *env, jobject this,
                                            jobject file)
{
    jboolean rv = JNI_FALSE;
    // TODO
    return rv;
}


JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_rename0(JNIEnv *env, jobject this,
                                    jobject from, jobject to)
{
    jboolean rv = JNI_FALSE;
    // TODO
    return rv;
}

JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_setLastModifiedTime(JNIEnv *env, jobject this,
                                                jobject file, jlong time)
{
    jboolean rv = JNI_FALSE;

    // TODO
    return rv;
}


JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_setReadOnly(JNIEnv *env, jobject this,
                                        jobject file)
{
    jboolean rv = JNI_FALSE;

    // TODO
    return rv;
}

JNIEXPORT jlong JNICALL
Java_java_io_UnixFileSystem_getSpace(JNIEnv *env, jobject this,
                                     jobject file, jint t)
{
    jlong rv = 0L;

    // TODO
    return rv;
}
