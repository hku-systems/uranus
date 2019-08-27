#ifndef SGX_STDLIB_UTIL_H
#define SGX_STDLIB_UTIL_H

#include <tlibc/stdlib.h>
#include <proxy/sgx_stdlib_t.h>
#include <sgx_trts.h>
#include <stdlib.h>
#include <sgx_stdio_util.h>
#include <string.h>


#include <fpu_control.h>


char *strdup (const char *s);

int atexit (void (*__func) (void));

char *getenv(const char *name);

int putenv(char *string);

int clearenv(void);

void exit(int status);


int setenv(const char *name, const char *value, int replace);

int unsetenv (const char *name);

int random();

void srandom (unsigned int seed);

#define srand(A) srandom(A)

int mkstemp(char *string);

char *mkdtemp(char *temp);


#endif 