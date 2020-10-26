//
// Created by jyjia on 2020/10/26.
//

extern "C" {
#include "setjmp.h"
#include "string.h"

void longjmp(struct __jmp_buf_tag __env[1], int __val) {
  char *NULL_PTR = NULL;
  *NULL_PTR;
}

int _setjmp (struct __jmp_buf_tag __env[1]) {

}

}

