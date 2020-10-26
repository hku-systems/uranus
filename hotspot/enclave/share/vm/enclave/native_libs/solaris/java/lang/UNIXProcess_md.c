/*
 * Copyright (c) 1995, 2013, Oracle and/or its affiliates. All rights reserved.
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

#undef  _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE 1

#include "jni.h"
#include "jvm.h"
#include "jvm_md.h"
#include "jni_util.h"
#include "io_util.h"

/*
 * Platform-specific support for java.lang.Process
 */
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

#if defined(__solaris__) || defined(_ALLBSD_SOURCE) || defined(_AIX)
#include <spawn.h>
#endif

#include "childproc.h"

/*
 * There are 4 possible strategies we might use to "fork":
 *
 * - fork(2).  Very portable and reliable but subject to
 *   failure due to overcommit (see the documentation on
 *   /proc/sys/vm/overcommit_memory in Linux proc(5)).
 *   This is the ancient problem of spurious failure whenever a large
 *   process starts a small subprocess.
 *
 * - vfork().  Using this is scary because all relevant man pages
 *   contain dire warnings, e.g. Linux vfork(2).  But at least it's
 *   documented in the glibc docs and is standardized by XPG4.
 *   http://www.opengroup.org/onlinepubs/000095399/functions/vfork.html
 *   On Linux, one might think that vfork() would be implemented using
 *   the clone system call with flag CLONE_VFORK, but in fact vfork is
 *   a separate system call (which is a good sign, suggesting that
 *   vfork will continue to be supported at least on Linux).
 *   Another good sign is that glibc implements posix_spawn using
 *   vfork whenever possible.  Note that we cannot use posix_spawn
 *   ourselves because there's no reliable way to close all inherited
 *   file descriptors.
 *
 * - clone() with flags CLONE_VM but not CLONE_THREAD.  clone() is
 *   Linux-specific, but this ought to work - at least the glibc
 *   sources contain code to handle different combinations of CLONE_VM
 *   and CLONE_THREAD.  However, when this was implemented, it
 *   appeared to fail on 32-bit i386 (but not 64-bit x86_64) Linux with
 *   the simple program
 *     Runtime.getRuntime().exec("/bin/true").waitFor();
 *   with:
 *     #  Internal Error (os_linux_x86.cpp:683), pid=19940, tid=2934639536
 *     #  Error: pthread_getattr_np failed with errno = 3 (ESRCH)
 *   We believe this is a glibc bug, reported here:
 *     http://sources.redhat.com/bugzilla/show_bug.cgi?id=10311
 *   but the glibc maintainers closed it as WONTFIX.
 *
 * - posix_spawn(). While posix_spawn() is a fairly elaborate and
 *   complicated system call, it can't quite do everything that the old
 *   fork()/exec() combination can do, so the only feasible way to do
 *   this, is to use posix_spawn to launch a new helper executable
 *   "jprochelper", which in turn execs the target (after cleaning
 *   up file-descriptors etc.) The end result is the same as before,
 *   a child process linked to the parent in the same way, but it
 *   avoids the problem of duplicating the parent (VM) process
 *   address space temporarily, before launching the target command.
 *
 * Based on the above analysis, we are currently using vfork() on
 * Linux and spawn() on other Unix systems, but the code to use clone()
 * and fork() remains.
 */


static void
setSIGCHLDHandler(JNIEnv *env)
{
    // TODO
}

static void*
xmalloc(JNIEnv *env, size_t size)
{
    void *p = malloc(size);
    if (p == NULL)
        JNU_ThrowOutOfMemoryError(env, NULL);
    return p;
}

#define NEW(type, n) ((type *) xmalloc(env, (n) * sizeof(type)))

/**
 * If PATH is not defined, the OS provides some default value.
 * Unfortunately, there's no portable way to get this value.
 * Fortunately, it's only needed if the child has PATH while we do not.
 */
static const char*
defaultPath(void)
{
    return ":/bin:/usr/bin";    /* glibc */
}

static const char*
effectivePath(void)
{
    // TODO
    return defaultPath();
}

static int
countOccurrences(const char *s, char c)
{
    int count;
    for (count = 0; *s != '\0'; s++)
        count += (*s == c);
    return count;
}

static const char * const *
effectivePathv(JNIEnv *env)
{
    char *p;
    int i;
    const char *path = effectivePath();
    int count = countOccurrences(path, ':') + 1;
    size_t pathvsize = sizeof(const char *) * (count+1);
    size_t pathsize = strlen(path) + 1;
    const char **pathv = (const char **) xmalloc(env, pathvsize + pathsize);

    if (pathv == NULL)
        return NULL;
    p = (char *) pathv + pathvsize;
    memcpy(p, path, pathsize);
    /* split PATH by replacing ':' with NULs; empty components => "." */
    for (i = 0; i < count; i++) {
        char *q = p + strcspn(p, ":");
        pathv[i] = (p == q) ? "." : p;
        *q = '\0';
        p = q + 1;
    }
    pathv[count] = NULL;
    return pathv;
}

JNIEXPORT void JNICALL
Java_java_lang_UNIXProcess_init(JNIEnv *env, jclass clazz)
{
    parentPathv = effectivePathv(env);
    CHECK_NULL(parentPathv);
    setSIGCHLDHandler(env);
}


#ifndef WIFEXITED
#define WIFEXITED(status) (((status)&0xFF) == 0)
#endif

#ifndef WEXITSTATUS
#define WEXITSTATUS(status) (((status)>>8)&0xFF)
#endif

#ifndef WIFSIGNALED
#define WIFSIGNALED(status) (((status)&0xFF) > 0 && ((status)&0xFF00) == 0)
#endif

#ifndef WTERMSIG
#define WTERMSIG(status) ((status)&0x7F)
#endif

/* Block until a child process exits and return its exit code.
   Note, can only be called once for any given pid. */
JNIEXPORT jint JNICALL
Java_java_lang_UNIXProcess_waitForProcessExit(JNIEnv* env,
                                              jobject junk,
                                              jint pid)
{
    return 0;
}

static const char *
getBytes(JNIEnv *env, jbyteArray arr)
{
    return arr == NULL ? NULL :
        (const char*) (*env)->GetByteArrayElements(env, arr, NULL);
}

static void
releaseBytes(JNIEnv *env, jbyteArray arr, const char* parr)
{
    if (parr != NULL)
        (*env)->ReleaseByteArrayElements(env, arr, (jbyte*) parr, JNI_ABORT);
}

static void
throwIOException(JNIEnv *env, int errnum, const char *defaultDetail)
{
    static const char * const format = "error=%d, %s";
    const char *detail = defaultDetail;
    char *errmsg;
    char tmpbuf[1024];
    jstring s;

    if (errnum != 0) {
        int ret = getErrorString(errnum, tmpbuf, sizeof(tmpbuf));
        if (ret != EINVAL)
            detail = tmpbuf;
    }
    /* ASCII Decimal representation uses 2.4 times as many bits as binary. */
    errmsg = NEW(char, strlen(format) + strlen(detail) + 3 * sizeof(errnum));
    if (errmsg == NULL)
        return;

    sprintf(errmsg, format, errnum, detail);
    s = JNU_NewStringPlatform(env, errmsg);
    if (s != NULL) {
        jobject x = JNU_NewObjectByName(env, "java/io/IOException",
                                        "(Ljava/lang/String;)V", s);
        if (x != NULL)
            (*env)->Throw(env, x);
    }
    free(errmsg);
}

#ifdef DEBUG_PROCESS
/* Debugging process code is difficult; where to write debug output? */
static void
debugPrint(char *format, ...)
{
    FILE *tty = fopen("/dev/tty", "w");
    va_list ap;
    va_start(ap, format);
    vfprintf(tty, format, ap);
    va_end(ap);
    fclose(tty);
}
#endif /* DEBUG_PROCESS */

static void
copyPipe(int from[2], int to[2])
{
    to[0] = from[0];
    to[1] = from[1];
}

/* arg is an array of pointers to 0 terminated strings. array is terminated
 * by a null element.
 *
 * *nelems and *nbytes receive the number of elements of array (incl 0)
 * and total number of bytes (incl. 0)
 * Note. An empty array will have one null element
 * But if arg is null, then *nelems set to 0, and *nbytes to 0
 */
static void arraysize(const char * const *arg, int *nelems, int *nbytes)
{
    int i, bytes, count;
    const char * const *a = arg;
    char *p;
    int *q;
    if (arg == 0) {
        *nelems = 0;
        *nbytes = 0;
        return;
    }
    /* count the array elements and number of bytes */
    for (count=0, bytes=0; *a != 0; count++, a++) {
        bytes += strlen(*a)+1;
    }
    *nbytes = bytes;
    *nelems = count+1;
}

/* copy the strings from arg[] into buf, starting at given offset
 * return new offset to next free byte
 */
static int copystrings(char *buf, int offset, const char * const *arg) {
    char *p;
    const char * const *a;
    int count=0;

    if (arg == 0) {
        return offset;
    }
    for (p=buf+offset, a=arg; *a != 0; a++) {
        int len = strlen(*a) +1;
        memcpy(p, *a, len);
        p += len;
        count += len;
    }
    return offset+count;
}

/**
 * We are unusually paranoid; use of clone/vfork is
 * especially likely to tickle gcc/glibc bugs.
 */
#ifdef __attribute_noinline__  /* See: sys/cdefs.h */
__attribute_noinline__
#endif

#define START_CHILD_USE_CLONE 0  /* clone() currently disabled; see above. */

#ifdef START_CHILD_USE_CLONE
static pid_t
cloneChild(ChildStuff *c) {
    assert(0);
    return -1;
}
#endif

static pid_t
vforkChild(ChildStuff *c) {
    return -1;
}

static pid_t
forkChild(ChildStuff *c) {
    return -1;
}

#if defined(__solaris__) || defined(_ALLBSD_SOURCE) || defined(_AIX)
static pid_t
spawnChild(JNIEnv *env, jobject process, ChildStuff *c, const char *helperpath) {
    pid_t resultPid;
    jboolean isCopy;
    int i, offset, rval, bufsize, magic;
    char *buf, buf1[16];
    char *hlpargs[2];
    SpawnInfo sp;

    /* need to tell helper which fd is for receiving the childstuff
     * and which fd to send response back on
     */
    snprintf(buf1, sizeof(buf1), "%d:%d", c->childenv[0], c->fail[1]);
    /* put the fd string as argument to the helper cmd */
    hlpargs[0] = buf1;
    hlpargs[1] = 0;

    /* Following items are sent down the pipe to the helper
     * after it is spawned.
     * All strings are null terminated. All arrays of strings
     * have an empty string for termination.
     * - the ChildStuff struct
     * - the SpawnInfo struct
     * - the argv strings array
     * - the envv strings array
     * - the home directory string
     * - the parentPath string
     * - the parentPathv array
     */
    /* First calculate the sizes */
    arraysize(c->argv, &sp.nargv, &sp.argvBytes);
    bufsize = sp.argvBytes;
    arraysize(c->envv, &sp.nenvv, &sp.envvBytes);
    bufsize += sp.envvBytes;
    sp.dirlen = c->pdir == 0 ? 0 : strlen(c->pdir)+1;
    bufsize += sp.dirlen;
    arraysize(parentPathv, &sp.nparentPathv, &sp.parentPathvBytes);
    bufsize += sp.parentPathvBytes;
    /* We need to clear FD_CLOEXEC if set in the fds[].
     * Files are created FD_CLOEXEC in Java.
     * Otherwise, they will be closed when the target gets exec'd */
    for (i=0; i<3; i++) {
        if (c->fds[i] != -1) {
            int flags = fcntl(c->fds[i], F_GETFD);
            if (flags & FD_CLOEXEC) {
                fcntl(c->fds[i], F_SETFD, flags & (~1));
            }
        }
    }

    rval = posix_spawn(&resultPid, helperpath, 0, 0, (char * const *) hlpargs, environ);

    if (rval != 0) {
        return -1;
    }

    /* now the lengths are known, copy the data */
    buf = NEW(char, bufsize);
    if (buf == 0) {
        return -1;
    }
    offset = copystrings(buf, 0, &c->argv[0]);
    offset = copystrings(buf, offset, &c->envv[0]);
    memcpy(buf+offset, c->pdir, sp.dirlen);
    offset += sp.dirlen;
    offset = copystrings(buf, offset, parentPathv);
    assert(offset == bufsize);

    magic = magicNumber();

    /* write the two structs and the data buffer */
    write(c->childenv[1], (char *)&magic, sizeof(magic)); // magic number first
    write(c->childenv[1], (char *)c, sizeof(*c));
    write(c->childenv[1], (char *)&sp, sizeof(sp));
    write(c->childenv[1], buf, bufsize);
    free(buf);

    /* In this mode an external main() in invoked which calls back into
     * childProcess() in this file, rather than directly
     * via the statement below */
    return resultPid;
}
#endif

/*
 * Start a child process running function childProcess.
 * This function only returns in the parent.
 */
static pid_t
startChild(JNIEnv *env, jobject process, ChildStuff *c, const char *helperpath) {
    return -1;
}

JNIEXPORT jint JNICALL
Java_java_lang_UNIXProcess_forkAndExec(JNIEnv *env,
                                       jobject process,
                                       jint mode,
                                       jbyteArray helperpath,
                                       jbyteArray prog,
                                       jbyteArray argBlock, jint argc,
                                       jbyteArray envBlock, jint envc,
                                       jbyteArray dir,
                                       jintArray std_fds,
                                       jboolean redirectErrorStream)
{
    return -1;
}

JNIEXPORT void JNICALL
Java_java_lang_UNIXProcess_destroyProcess(JNIEnv *env,
                                          jobject junk,
                                          jint pid,
                                          jboolean force)
{
    return;
}
