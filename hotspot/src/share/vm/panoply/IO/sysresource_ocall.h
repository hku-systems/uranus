#ifndef PANOPLY_SYSRESOURCE_H
#define PANOPLY_SYSRESOURCE_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

int ocall_prlimit (__pid_t pid, enum __rlimit_resource resource, const struct rlimit *new_limit, struct rlimit *old_limit);

int ocall_getrlimit(int resource, struct rlimit *rlim);

int ocall_setrlimit(int resource, const struct rlimit *rlim);

#if defined(__cplusplus)
}
#endif
#endif

