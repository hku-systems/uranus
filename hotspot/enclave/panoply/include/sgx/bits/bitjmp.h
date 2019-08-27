//
// Created by max on 12/30/17.
//

#ifndef HOTSPOT_BITJMP_H
#define HOTSPOT_BITJMP_H
/*#if !defined _SETJMP_H && !defined _PTHREAD_H
# error "Never include <bits/setjmp.h> directly; use <setjmp.h> instead."
#endif*/

#include "wordsize.h"

#ifndef _ASM

# if __WORDSIZE == 64
typedef long int __jmp_buf[8];
# elif defined  __x86_64__
__extension__ typedef long long int __jmp_buf[8];
# else
typedef int __jmp_buf[6];
# endif

#endif
#endif //HOTSPOT_BITJMP_H
