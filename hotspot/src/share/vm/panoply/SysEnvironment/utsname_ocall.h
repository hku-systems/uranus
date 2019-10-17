#ifndef PANOPLY_UTSNAME_H
#define PANOPLY_UTSNAME_H
#if defined(__cplusplus)
extern "C" {
#endif

#include <sys/utsname.h>

extern void increase_ocall_count();

int ocall_uname(struct utsname *name);
#if defined(__cplusplus)
}
#endif
#endif


