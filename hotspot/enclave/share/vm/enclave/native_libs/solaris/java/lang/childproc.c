/*
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "childproc.h"


ssize_t
restartableWrite(int fd, const void *buf, size_t count)
{
    ssize_t result;
    RESTARTABLE(write(fd, buf, count), result);
    return result;
}

int
restartableDup2(int fd_from, int fd_to)
{
    return -1;
}

int
closeSafely(int fd)
{
    return (fd == -1) ? 0 : close(fd);
}

int
isAsciiDigit(char c)
{
  return c >= '0' && c <= '9';
}

#ifdef _ALLBSD_SOURCE
#define FD_DIR "/dev/fd"
#define dirent64 dirent
#define readdir64 readdir
#elif defined(_AIX)
/* AIX does not understand '/proc/self' - it requires the real process ID */
#define FD_DIR aix_fd_dir
#else
#define FD_DIR "/proc/self/fd"
#endif

int
closeDescriptors(void)
{
    // TODO
    return 1;
}

int
moveDescriptor(int fd_from, int fd_to)
{
    if (fd_from != fd_to) {
        if ((restartableDup2(fd_from, fd_to) == -1) ||
            (close(fd_from) == -1))
            return -1;
    }
    return 0;
}

int
magicNumber() {
    return 43110;
}

/*
 * Reads nbyte bytes from file descriptor fd into buf,
 * The read operation is retried in case of EINTR or partial reads.
 *
 * Returns number of bytes read (normally nbyte, but may be less in
 * case of EOF).  In case of read errors, returns -1 and sets errno.
 */
ssize_t
readFully(int fd, void *buf, size_t nbyte)
{
    ssize_t remaining = nbyte;
    for (;;) {
        ssize_t n = read(fd, buf, remaining);
        if (n == 0) {
            return nbyte - remaining;
        } else if (n > 0) {
            remaining -= n;
            if (remaining <= 0)
                return nbyte;
            /* We were interrupted in the middle of reading the bytes.
             * Unlikely, but possible. */
            buf = (void *) (((char *)buf) + n);
        } else if (errno == EINTR) {
            /* Strange signals like SIGJVM1 are possible at any time.
             * See http://www.dreamsongs.com/WorseIsBetter.html */
        } else {
            return -1;
        }
    }
}

void
initVectorFromBlock(const char**vector, const char* block, int count)
{
    int i;
    const char *p;
    for (i = 0, p = block; i < count; i++) {
        /* Invariant: p always points to the start of a C string. */
        vector[i] = p;
        while (*(p++));
    }
    vector[count] = NULL;
}

/**
 * Exec FILE as a traditional Bourne shell script (i.e. one without #!).
 * If we could do it over again, we would probably not support such an ancient
 * misfeature, but compatibility wins over sanity.  The original support for
 * this was imported accidentally from execvp().
 */
void
execve_as_traditional_shell_script(const char *file,
                                   const char *argv[],
                                   const char *const envp[])
{
    // TODO
}

/**
 * Like execve(2), except that in case of ENOEXEC, FILE is assumed to
 * be a shell script and the system default shell is invoked to run it.
 */
void
execve_with_shell_fallback(int mode, const char *file,
                           const char *argv[],
                           const char *const envp[])
{
    // TODO
}

/**
 * 'execvpe' should have been included in the Unix standards,
 * and is a GNU extension in glibc 2.10.
 *
 * JDK_execvpe is identical to execvp, except that the child environment is
 * specified via the 3rd argument instead of being inherited from environ.
 */
void
JDK_execvpe(int mode, const char *file,
            const char *argv[],
            const char *const envp[])
{
    // TODO
}

/**
 * Child process after a successful fork() or clone().
 * This function must not return, and must be prepared for either all
 * of its address space to be shared with its parent, or to be a copy.
 * It must not modify global variables such as "environ".
 */
int
childProcess(void *arg)
{
  // TODO
  return -1;
}
