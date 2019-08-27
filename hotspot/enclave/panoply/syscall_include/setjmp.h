//
// Created by max on 12/30/17.
//

#ifndef HOTSPOT_SETJMP_H
#define HOTSPOT_SETJMP_H

#include "../include/sgx/bits/sigset.h"
#include "../include/sgx/bits/bitjmp.h"

struct __jmp_buf_tag
{
    /* NOTE: The machine-dependent definitions of `__sigsetjmp'
       assume that a `jmp_buf' begins with a `__jmp_buf' and that
       `__mask_was_saved' follows it.  Do not move these members
       or add others before it.  */
    __jmp_buf __jmpbuf;		/* Calling environment.  */
    int __mask_was_saved;	/* Saved the signal mask?  */
    __sigset_t __saved_mask;	/* Saved signal mask.  */
};

typedef struct __jmp_buf_tag sigjmp_buf[1];

#endif //HOTSPOT_SETJMP_H
