#ifndef SGX_STDLIB_UTIL_H
#define SGX_STDLIB_UTIL_H

#include <tlibc/stdlib.h>
#include <sgx_trts.h>
#include <sgx_stdio_util.h>
#include <string.h>


#include <fpu_control.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif 