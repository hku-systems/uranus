#ifndef PANOPLY_SYSCTL_H
#define PANOPLY_SYSCTL_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

int ocall_sysctl (int *name, int nlen, void *oldval, size_t *oldlenp, void *newval, size_t newlen);

#if defined(__cplusplus)
}
#endif
#endif
