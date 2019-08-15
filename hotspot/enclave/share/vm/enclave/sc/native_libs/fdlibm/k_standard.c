
/*
 * Copyright (c) 1998, 2001, Oracle and/or its affiliates. All rights reserved.
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

#include "fdlibm.h"
#include <errno.h>

#ifndef _USE_WRITE
#include <stdio.h>                      /* fputs(), stderr */
#define WRITE2(u,v)     sprintf(NULL, u)
#else   /* !defined(_USE_WRITE) */
#include <unistd.h>                     /* write */
#define WRITE2(u,v)     write(2, u, v)
#undef fflush
#endif  /* !defined(_USE_WRITE) */

static double zero = 0.0;       /* used as const */

/*
 * Standard conformance (non-IEEE) on exception cases.
 * Mapping:
 *      1 -- acos(|x|>1)
 *      2 -- asin(|x|>1)
 *      3 -- atan2(+-0,+-0)
 *      4 -- hypot overflow
 *      5 -- cosh overflow
 *      6 -- exp overflow
 *      7 -- exp underflow
 *      8 -- y0(0)
 *      9 -- y0(-ve)
 *      10-- y1(0)
 *      11-- y1(-ve)
 *      12-- yn(0)
 *      13-- yn(-ve)
 *      14-- lgamma(finite) overflow
 *      15-- lgamma(-integer)
 *      16-- log(0)
 *      17-- log(x<0)
 *      18-- log10(0)
 *      19-- log10(x<0)
 *      20-- pow(0.0,0.0)
 *      21-- pow(x,y) overflow
 *      22-- pow(x,y) underflow
 *      23-- pow(0,negative)
 *      24-- pow(neg,non-integral)
 *      25-- sinh(finite) overflow
 *      26-- sqrt(negative)
 *      27-- fmod(x,0)
 *      28-- remainder(x,0)
 *      29-- acosh(x<1)
 *      30-- atanh(|x|>1)
 *      31-- atanh(|x|=1)
 *      32-- scalb overflow
 *      33-- scalb underflow
 *      34-- j0(|x|>X_TLOSS)
 *      35-- y0(x>X_TLOSS)
 *      36-- j1(|x|>X_TLOSS)
 *      37-- y1(x>X_TLOSS)
 *      38-- jn(|x|>X_TLOSS, n)
 *      39-- yn(x>X_TLOSS, n)
 *      40-- gamma(finite) overflow
 *      41-- gamma(-integer)
 *      42-- pow(NaN,0.0)
 */


#ifdef __STDC__
        double __kernel_standard(double x, double y, int type)
#else
        double __kernel_standard(x,y,type)
        double x,y; int type;
#endif
{
    return 0;
}
