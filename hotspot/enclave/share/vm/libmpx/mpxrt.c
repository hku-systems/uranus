/* mpxrt.c                  -*-C++-*-
 *
 *************************************************************************
 *
 *  @copyright
 *  Copyright (C) 2014, Intel Corporation
 *  All rights reserved.
 *
 *  @copyright
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  @copyright
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 *  WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 **************************************************************************/

#define __STDC_FORMAT_MACROS
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "mpxrt.h"

#define MPX_ENABLE_BIT_NO 0
#define BNDPRESERVE_BIT_NO 1

struct xsave_hdr_struct
{
  uint64_t xstate_bv;
  uint64_t reserved1[2];
  uint64_t reserved2[5];
} __attribute__ ((packed));

struct bndregs_struct
{
  uint64_t bndregs[8];
} __attribute__ ((packed));

struct bndcsr_struct {
	uint64_t cfg_reg_u;
	uint64_t status_reg;
} __attribute__((packed));

struct xsave_struct
{
  uint8_t fpu_sse[512];
  struct xsave_hdr_struct xsave_hdr;
  uint8_t ymm[256];
  uint8_t lwp[128];
  struct bndregs_struct bndregs;
  struct bndcsr_struct bndcsr;
} __attribute__ ((packed));

/* Following vars are initialized at process startup only
   and thus are considered to be thread safe.  */
static void *l1base = NULL;
static int bndpreserve = 1;
static int enable = 1;

/* Var holding number of occured BRs.  It is modified from
   signal handler only and thus it should be thread safe.  */
static uint64_t num_bnd_chk = 0;

static inline void
xrstor_state (struct xsave_struct *fx, uint64_t mask)
{
  uint32_t lmask = mask;
  uint32_t hmask = mask >> 32;

  asm volatile (".byte " REX_PREFIX "0x0f,0xae,0x2f\n\t"
		: : "D" (fx), "m" (*fx), "a" (lmask), "d" (hmask)
		:   "memory");
}

static inline void
xsave_state (struct xsave_struct *fx, uint64_t mask)
{
  uint32_t lmask = mask;
  uint32_t hmask = mask >> 32;

  asm volatile (".byte " REX_PREFIX "0x0f,0xae,0x27\n\t"
		: : "D" (fx), "m" (*fx), "a" (lmask), "d" (hmask)
		:   "memory");
}

void
enable_mpx (void)
{
  uint8_t __attribute__ ((__aligned__ (64))) buffer[4096];
  struct xsave_struct *xsave_buf = (struct xsave_struct *)buffer;

  memset (buffer, 0, sizeof (buffer));
  xrstor_state (xsave_buf, 0x18);

  l1base=NULL;

  /* Enable MPX.  */
  xsave_buf->xsave_hdr.xstate_bv = 0x10;
  xsave_buf->bndcsr.cfg_reg_u = (unsigned long)l1base;
  xsave_buf->bndcsr.cfg_reg_u |= enable << MPX_ENABLE_BIT_NO;
  xsave_buf->bndcsr.cfg_reg_u |= bndpreserve << BNDPRESERVE_BIT_NO;
  xsave_buf->bndcsr.status_reg = 0;

  xrstor_state (xsave_buf, 0x10);
}

void
disable_mpx (void)
{
  uint8_t __attribute__ ((__aligned__ (64))) buffer[4096];
  struct xsave_struct *xsave_buf = (struct xsave_struct *)buffer;

  memset(buffer, 0, sizeof(buffer));
  xrstor_state(xsave_buf, 0x18);

  /* Disable MPX.  */
  xsave_buf->xsave_hdr.xstate_bv = 0x10;
  xsave_buf->bndcsr.cfg_reg_u = 0;
  xsave_buf->bndcsr.status_reg = 0;

  xrstor_state(xsave_buf, 0x10);
}
