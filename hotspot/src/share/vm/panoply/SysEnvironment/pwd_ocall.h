#ifndef PANOPLY_PWD_H
#define PANOPLY_PWD_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

struct passwd *ocall_getpwuid(uid_t uid);

struct passwd *ocall_getpwnam(const char *name);

int ocall_getpwnam_r(const char *name, struct passwd *pwd,
                     char *buf, size_t buflen, struct passwd **result);

#if defined(__cplusplus)
}
#endif
#endif