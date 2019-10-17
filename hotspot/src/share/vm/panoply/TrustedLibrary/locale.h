#ifndef PANOPLY_LOCALE_H
#define PANOPLY_LOCALE_H
#if defined(__cplusplus)
extern "C" {
#endif
#include <sys/time.h>
#include <locale.h>

char *wrapper_setlocale(int category, const char *locale);

#if defined(__cplusplus)
}
#endif
#endif