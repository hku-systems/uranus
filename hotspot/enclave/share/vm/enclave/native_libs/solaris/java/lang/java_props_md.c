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

#if defined(__linux__) || defined(_ALLBSD_SOURCE)
#include <stdio.h>
#include <ctype.h>
#endif
#include <pwd.h>
//#include <locale.h>
#define ARCHPROPNAME "arch"
#ifndef ARCHPROPNAME
#error "The macro ARCHPROPNAME has not been defined"
#endif
#include <sys/utsname.h>        /* For os_name and os_version */
//#include <langinfo.h>           /* For nl_langinfo */
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

#include "locale_str.h"
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

static int ParseLocale(JNIEnv* env, int cat, char ** std_language, char ** std_script,
                       char ** std_country, char ** std_variant, char ** std_encoding) {
    return 1;
}

#ifdef JAVASE_EMBEDDED
/* Determine the default embedded toolkit based on whether libawt_xawt
 * exists in the JRE. This can still be overridden by -Dawt.toolkit=XXX
 */
static char* getEmbeddedToolkit() {
    Dl_info dlinfo;
    char buf[MAXPATHLEN];
    int32_t len;
    char *p;
    struct stat statbuf;

    /* Get address of this library and the directory containing it. */
    dladdr((void *)getEmbeddedToolkit, &dlinfo);
    realpath((char *)dlinfo.dli_fname, buf);
    len = strlen(buf);
    p = strrchr(buf, '/');
    /* Default AWT Toolkit on Linux and Solaris is XAWT (libawt_xawt.so). */
    strncpy(p, "/libawt_xawt.so", MAXPATHLEN-len-1);
    /* Check if it exists */
    if (stat(buf, &statbuf) == -1 && errno == ENOENT) {
        /* No - this is a reduced-headless-jre so use special HToolkit */
        return "sun.awt.HToolkit";
    }
    else {
        /* Yes - this is a headful JRE so fallback to SE defaults */
        return NULL;
    }
}
#endif

/* This function gets called very early, before VM_CALLS are setup.
 * Do not use any of the VM_CALLS entries!!!
 */
java_props_t *
GetJavaProperties(JNIEnv *env)
{
    static java_props_t sprops;
    char *v; /* tmp var */

    if (sprops.user_dir) {
        return &sprops;
    }

    /* tmp dir */
    sprops.tmp_dir = P_tmpdir;
#ifdef MACOSX
    /* darwin has a per-user temp dir */
    static char tmp_path[PATH_MAX];
    int pathSize = confstr(_CS_DARWIN_USER_TEMP_DIR, tmp_path, PATH_MAX);
    if (pathSize > 0 && pathSize <= PATH_MAX) {
        sprops.tmp_dir = tmp_path;
    }
#endif /* MACOSX */

    /* Printing properties */
#ifdef MACOSX
    sprops.printerJob = "sun.lwawt.macosx.CPrinterJob";
#else
    sprops.printerJob = "sun.print.PSPrinterJob";
#endif

    /* patches/service packs installed */
    sprops.patch_level = "unknown";

    /* Java 2D/AWT properties */
#ifdef MACOSX
    // Always the same GraphicsEnvironment and Toolkit on Mac OS X
    sprops.graphics_env = "sun.awt.CGraphicsEnvironment";
    sprops.awt_toolkit = "sun.lwawt.macosx.LWCToolkit";

    // check if we're in a GUI login session and set java.awt.headless=true if not
    sprops.awt_headless = isInAquaSession() ? NULL : "true";
#else
    sprops.graphics_env = "sun.awt.X11GraphicsEnvironment";
#ifdef JAVASE_EMBEDDED
    sprops.awt_toolkit = getEmbeddedToolkit();
    if (sprops.awt_toolkit == NULL) // default as below
#endif
    sprops.awt_toolkit = "sun.awt.X11.XToolkit";
#endif

    /* This is used only for debugging of font problems. */
    sprops.font_dir = NULL;

#ifdef SI_ISALIST
    /* supported instruction sets */
    {
        char list[258];
        sysinfo(SI_ISALIST, list, sizeof(list));
        sprops.cpu_isalist = strdup(list);
    }
#else
    sprops.cpu_isalist = NULL;
#endif

    /* endianness of platform */
    {
        unsigned int endianTest = 0xff000000;
        if (((char*)(&endianTest))[0] != 0)
            sprops.cpu_endian = "big";
        else
            sprops.cpu_endian = "little";
    }

    /* os properties */
    {

        sprops.os_name = "enclave.os";
        sprops.os_version = "0.1";
        sprops.os_arch = HOTSPOT_BUILD_TARGET;
        sprops.desktop = NULL;
    }

    /* ABI property (optional) */
#ifdef JDK_ARCH_ABI_PROP_NAME
    sprops.sun_arch_abi = JDK_ARCH_ABI_PROP_NAME;
#endif

    /* Determine the language, country, variant, and encoding from the host,
     * and store these in the user.language, user.country, user.variant and
     * file.encoding system properties. */
    sprops.language = "en";
    sprops.encoding = "ISO8859-1";
    sprops.display_language = sprops.language;
    sprops.display_script = sprops.script;
    sprops.display_country = sprops.country;
    sprops.display_variant = sprops.variant;

    /* ParseLocale failed with OOME */
    JNU_CHECK_EXCEPTION_RETURN(env, NULL);

#ifdef MACOSX
    sprops.sun_jnu_encoding = "UTF-8";
#else
    sprops.sun_jnu_encoding = sprops.encoding;
#endif

#ifdef _ALLBSD_SOURCE
#if BYTE_ORDER == _LITTLE_ENDIAN
     sprops.unicode_encoding = "UnicodeLittle";
 #else
     sprops.unicode_encoding = "UnicodeBig";
 #endif
#else /* !_ALLBSD_SOURCE */
#ifdef __linux__
#if __BYTE_ORDER == __LITTLE_ENDIAN
    sprops.unicode_encoding = "UnicodeLittle";
#else
    sprops.unicode_encoding = "UnicodeBig";
#endif
#else
    sprops.unicode_encoding = "UnicodeBig";
#endif
#endif /* _ALLBSD_SOURCE */

    /* user properties */
    {
        sprops.user_name = "?";
        sprops.user_home = NULL;
        if (sprops.user_home == NULL) {
            sprops.user_home = "?";
        }
    }

    /* User TIMEZONE */
    {
        /*
         * We defer setting up timezone until it's actually necessary.
         * Refer to TimeZone.getDefault(). However, the system
         * property is necessary to be able to be set by the command
         * line interface -D. Here temporarily set a null string to
         * timezone.
         */
        sprops.timezone = "";
    }

    /* Current directory */
    {
        char buf[MAXPATHLEN] = "./";
        errno = 0;
        sprops.user_dir = strdup(buf);
    }

    sprops.file_separator = "/";
    sprops.path_separator = ":";
    sprops.line_separator = "\n";

#ifdef MACOSX
    setProxyProperties(&sprops);
#endif

    return &sprops;
}

jstring
GetStringPlatform(JNIEnv *env, nchar* cstr)
{
    return JNU_NewStringPlatform(env, cstr);
}
