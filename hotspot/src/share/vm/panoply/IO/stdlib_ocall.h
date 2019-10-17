#ifndef PANOPLY_STDLIB_H
#define PANOPLY_STDLIB_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

char *ocall_getenv(const char *name);

int ocall_putenv(char *string);

int ocall_clearenv(void);

int ocall_setenv(const char *name, const char *value, int replace);

int ocall_unsetenv (const char *name);

int ocall_mkstemp(char *temp);

char *ocall_mkdtemp(char *temp);

#if defined(__cplusplus)
}
#endif
#endif

