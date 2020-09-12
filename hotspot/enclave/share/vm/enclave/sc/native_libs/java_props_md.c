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


#include <stdio.h>
#include <ctype.h>
#include <sys/utsname.h>        /* For os_name and os_version */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/param.h>
#include <time.h>
#include <errno.h>

#ifdef MACOSX
#include "java_props_macosx.h"
#endif

#if defined(_ALLBSD_SOURCE)
#if !defined(P_tmpdir)
#include <paths.h>
#define P_tmpdir _PATH_VARTMP
#endif
#endif

#include "java_props.h"

#if !defined(_ALLBSD_SOURCE)
#ifdef __linux__
  #ifndef CODESET
  #define CODESET _NL_CTYPE_CODESET_NAME
  #endif
#else
#ifdef ALT_CODESET_KEY
#define CODESET ALT_CODESET_KEY
#endif
#endif
#endif /* !_ALLBSD_SOURCE */

#ifdef JAVASE_EMBEDDED
#include <dlfcn.h>
#include <sys/stat.h>
#endif

/* Take an array of string pairs (map of key->value) and a string (key).
 * Examine each pair in the map to see if the first string (key) matches the
 * string.  If so, store the second string of the pair (value) in the value and
 * return 1.  Otherwise do nothing and return 0.  The end of the map is
 * indicated by an empty string at the start of a pair (key of "").
 */
static int
mapLookup(char* map[], const char* key, char** value) {
    int i;
    for (i = 0; strcmp(map[i], ""); i += 2){
        if (!strcmp(key, map[i])){
            *value = map[i + 1];
            return 1;
        }
    }
    return 0;
}

#ifndef P_tmpdir
#define P_tmpdir "/var/tmp"
#endif



/* This function gets called very early, before VM_CALLS are setup.
 * Do not use any of the VM_CALLS entries!!!
 */
java_props_t *
GetJavaProperties(JNIEnv *env)
{
 return NULL;
}

jstring
GetStringPlatform(JNIEnv *env, nchar* cstr)
{
    return JNU_NewStringPlatform(env, cstr);
}
