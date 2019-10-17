#ifndef PANOPLY_GRP_H
#define PANOPLY_GRP_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

struct group *ocall_getgrgid(gid_t gid);

int ocall_initgroups(const char *user, gid_t group);

#if defined(__cplusplus)
}
#endif
#endif