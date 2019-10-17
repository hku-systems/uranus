#ifndef PANOPLY_SYSIOCTL_H
#define PANOPLY_SYSIOCTL_H
#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

int ocall_ioctl(int fd, unsigned long request, void* arguments);

#if defined(__cplusplus)
}
#endif
#endif